// SDL3 frontend. This file owns the window, input and drawing; the simulation
// lives in peo::core::World and never sees SDL. Keep it thin: anything with
// logic in it belongs in core where it can be tested headlessly.
//
// Turn-based (D-002): a key press becomes exactly one turn; nothing advances on
// a timer. While the player thinks, one worker thread speculates the next turn
// (PEO-007); input commits it. Core spawns no threads: this file owns the only one.

#include "peo/core/turn_input.hpp"
#include "peo/core/world.hpp"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

#include "thread_pool.hpp"

namespace {

using namespace peo::core;

constexpr int kCell = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE; // 8 px glyphs
/// The seed with no argument, or with one that is not a seed (PEO-075).
constexpr Seed kDefaultSeed = 1;
/// The game's wind (PEO-048): each stage blows its own way at up to full strength, and
/// two gust rounds a update carry scent ahead downwind.
constexpr std::int32_t kGameWindMax = kWindFull;
constexpr int kGameWindGust = 2;
/// Compass points, clockwise from east as Wind::toward_degrees runs (y down).
constexpr std::array<const char*, 8> kCompass{"E", "SE", "S", "SW", "W", "NW", "N", "NE"};
constexpr std::int32_t kCompassStep = kDegreesPerTurn / static_cast<std::int32_t>(kCompass.size());

/// Threads that run core's pieces (PEO-080): the hardware's, less one for the main
/// thread, at least one. `--threads N` overrides it; 1 is serial.
std::size_t default_threads() noexcept {
    const unsigned hardware = std::thread::hardware_concurrency();
    return hardware > 1 ? hardware - 1 : 1;
}

WorldParams game_params() noexcept {
    WorldParams params;
    params.wind_max = kGameWindMax;
    params.scent.gust = kGameWindGust;
    return params;
}
constexpr int kScale = 2;
constexpr int kHudRows = 2; // a status line and a key-hint line (PEO-062)
/// SDL_AppIterate pacing. Nothing changes between key presses, so sleep until an
/// event arrives ("waitevent", SDL 3.4+). Older SDL parses that string as 0 and
/// would spin, so there it gets a frame cap instead. The SDL fetched from source
/// (cmake/FindOrFetchSDL3.cmake) is 3.4, so the cap only applies when find_package
/// picks up an older installed SDL.
constexpr int kWaitEventMinVersion = SDL_VERSIONNUM(3, 4, 0);
constexpr const char* kFallbackIterateHz = "60";

/// Timer delay floor: SDL treats a 0 ns timer oddly on some backends.
constexpr Uint64 kMinWakeNs = 1;

/// Scent view bands, log-spaced relative to the field's current maximum so the
/// view reads the same at any deposit scale (the geodesic field's samples, D-024). Strongest first;
/// none uses @, which is the player's glyph.
struct ScentBand {
    float fraction_of_max;
    char glyph;
};
constexpr std::array<ScentBand, 3> kScentBands{{{1e-1F, '*'}, {1e-3F, '+'}, {1e-5F, ':'}}};

char scent_glyph(std::int32_t scent, std::int32_t max_scent) {
    for (const ScentBand& band : kScentBands) {
        if (scent > 0 && static_cast<float>(scent) >= band.fraction_of_max * static_cast<float>(max_scent)) {
            return band.glyph;
        }
    }
    return '.';
}

/// Runs World::speculate() on a worker while the world is idle. The worker only
/// reads the World; the main thread mutates it only after quiesce(), so the two
/// never touch it at once. The buffer is reused, so a turn allocates nothing.
class Speculator {
public:
    explicit Speculator(const World& world)
        : world_(&world), worker_([this](std::stop_token st) { run(st); }) {}

    /// Start speculating the current turn. Call after every change to the world.
    void request() {
        {
            const std::lock_guard lock(mutex_);
            ready_ = false;
            requested_ = true;
        }
        wake_.notify_all();
    }

    /// Was a speculation already finished when the player acted?
    [[nodiscard]] bool ready_now() {
        const std::lock_guard lock(mutex_);
        return ready_;
    }

    /// Stop the worker touching the world: cancel a queued request, wait out one in
    /// flight. Returns the finished speculation, if any; valid until request().
    [[nodiscard]] Speculation* quiesce() {
        std::unique_lock lock(mutex_);
        requested_ = false;
        wake_.wait(lock, [this] { return !busy_; });
        return ready_ ? &buffer_ : nullptr;
    }

private:
    void run(std::stop_token st) {
        std::unique_lock lock(mutex_);
        while (wake_.wait(lock, st, [this] { return requested_; })) {
            requested_ = false;
            busy_ = true;
            lock.unlock();
            world_->speculate(buffer_);
            lock.lock();
            busy_ = false;
            ready_ = true;
            wake_.notify_all();
        }
    }

    const World* world_;
    std::mutex mutex_;
    std::condition_variable_any wake_;
    bool requested_ = false;
    bool busy_ = false;
    bool ready_ = false;
    Speculation buffer_;
    std::jthread worker_; // last: joins before the state above is destroyed
};

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    /// Core's executor (D-035): before world and speculator, so destroyed after them.
    std::optional<peo::app::ThreadPool> pool;
    std::optional<World> world;
    std::optional<Speculator> speculator; // after world: destroyed (joined) first
    bool show_scent = false;
    /// R toggles it (D-015): steps take kRunStepSeconds. Kept across stages.
    bool running = false;
    /// Redraw only when something changed (a turn, a view toggle, an expose).
    bool dirty = true;
    /// PEO-007 manual test: was the last turn's speculation ready at input?
    bool last_hit = false;
    unsigned long long misses = 0;
    /// Turn keys to turns (D-032): taps wait up to 3 deep, played out at 3 a second.
    TurnInput input;
    /// A registered SDL event type the wake timer pushes, and the pending timer (0: none).
    Uint32 wake_event = 0;
    SDL_TimerID wake_timer = 0;
};

/// Spend one turn on `action`: commit the speculation if there is one, else step.
/// A speculation is per update (D-015): an action that crosses no update boundary
/// leaves it valid, so a new one is requested only when the update or stage moved
/// on, or when none is ready (quiesce may have cancelled a queued request).
void take_turn(App& app, Action action) {
    const bool hit = app.speculator->ready_now();
    const Tick update_before = app.world->updates();
    const std::uint32_t stage_before = app.world->stage_index();
    if (Speculation* spec = app.speculator->quiesce()) {
        app.world->commit(*spec, action);
    } else {
        app.world->step(action);
    }
    app.last_hit = hit;
    app.misses += hit ? 0U : 1U;
    const bool moved_on = app.world->updates() != update_before || app.world->stage_index() != stage_before;
    if (moved_on || !app.speculator->ready_now()) {
        app.speculator->request();
    }
}

void draw(App& app) {
    const World& world = *app.world;
    const Stage& stage = world.stage();
    SDL_SetRenderDrawColor(app.renderer, 8, 8, 12, 255);
    SDL_RenderClear(app.renderer);
    SDL_SetRenderScale(app.renderer, kScale, kScale);

    const int w = stage.spec.width;
    const int h = stage.spec.height;
    std::string row(static_cast<std::size_t>(w), ' ');

    // Map + optional scent heat. The player's @ is drawn last and sits on top.
    const ScentWave& scent = world.scent();
    std::int32_t max_scent = 0;
    for (int y = 0; app.show_scent && y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            max_scent = std::max(max_scent, scent.sample({x, y}));
        }
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            char c = stage.blocked.at(x, y) ? '#' : '.';
            if (app.show_scent && !stage.blocked.at(x, y)) {
                c = scent_glyph(scent.sample({x, y}), max_scent);
            }
            row[static_cast<std::size_t>(x)] = c;
        }
        SDL_SetRenderDrawColor(app.renderer, 90, 90, 100, 255);
        SDL_RenderDebugText(app.renderer, 0.0F, static_cast<float>((y + kHudRows) * kCell), row.c_str());
    }

    auto glyph = [&](Vec2i p, const char* s, Uint8 r, Uint8 g, Uint8 b) {
        SDL_SetRenderDrawColor(app.renderer, r, g, b, 255);
        SDL_RenderDebugText(app.renderer, static_cast<float>(p.x * kCell),
                            static_cast<float>((p.y + kHudRows) * kCell), s);
    };
    glyph(stage.exit, ">", 120, 200, 255);
    for (const Dead& d : world.horde()) {
        glyph(d.pos, "d", 220, 60, 60);
    }
    glyph(world.player(), "@", 255, 255, 255);

    // Two lines, each under the default stage's 80 columns. The wind is named by where
    // it blows to, so "to E" is downwind east.
    const Wind wind = world.wind();
    const auto point = static_cast<std::size_t>(((wind.toward_degrees + kCompassStep / 2) / kCompassStep) %
                                                static_cast<std::int32_t>(kCompass.size()));
    char status[80];
    std::snprintf(
        status, sizeof status, "stage %u  turn %llu  %s  dead %zu  wind to %s %d  spec:%s (miss %llu)",
        world.stage_index(), static_cast<unsigned long long>(world.turn()), app.running ? "run" : "walk",
        world.horde().size(), wind.intensity > 0 ? kCompass[point] : "-", wind.intensity,
        app.last_hit ? "hit" : "miss", app.misses);
    static constexpr const char* kKeyHints =
        "[arrows/wasd] move [space/.] wait [r] run [shift+s] scent [n] next";
    SDL_SetRenderDrawColor(app.renderer, 200, 200, 120, 255);
    SDL_RenderDebugText(app.renderer, 0.0F, 0.0F, status);
    SDL_SetRenderDrawColor(app.renderer, 140, 140, 100, 255);
    SDL_RenderDebugText(app.renderer, 0.0F, static_cast<float>(kCell), kKeyHints);

    SDL_SetRenderScale(app.renderer, 1.0F, 1.0F);
    SDL_RenderPresent(app.renderer);
}

/// Map a key to the action it spends a turn on, if any. Running shortens steps
/// only; a wait lasts kWaitSeconds either way.
std::optional<Action> action_for(SDL_Keycode key, bool running) {
    const Seconds step = running ? kRunStepSeconds : kStepSeconds;
    switch (key) {
    case SDLK_UP:
    case SDLK_W:
        return Action::step({0, -1}, step);
    case SDLK_DOWN:
    case SDLK_S:
        return Action::step({0, 1}, step);
    case SDLK_LEFT:
    case SDLK_A:
        return Action::step({-1, 0}, step);
    case SDLK_RIGHT:
    case SDLK_D:
        return Action::step({1, 0}, step);
    case SDLK_SPACE:
    case SDLK_PERIOD:
        return Action::wait();
    default:
        return std::nullopt;
    }
}

/// Timer thread: wake the main loop to drain a waiting tap. One shot.
Uint64 SDLCALL wake(void* userdata, SDL_TimerID /*timer*/, Uint64 /*interval*/) {
    SDL_Event e{};
    e.type = static_cast<Uint32>(reinterpret_cast<std::uintptr_t>(userdata));
    SDL_PushEvent(&e);
    return 0;
}

/// Take the turn that is due, if any, and while taps still wait arrange one wake-up
/// at the next due time; nothing waiting means no timer, so the loop sleeps (PEO-036).
void drain(App& app) {
    const Uint64 now = SDL_GetTicksNS();
    if (const std::optional<Action> action = app.input.take(now)) {
        take_turn(app, *action);
        app.dirty = true;
    }
    if (const std::optional<Nanos> due = app.input.next_due(); due && app.wake_timer == 0) {
        const Uint64 delay = std::max(*due > now ? *due - now : 0, kMinWakeNs);
        app.wake_timer =
            SDL_AddTimerNS(delay, wake, reinterpret_cast<void*>(static_cast<std::uintptr_t>(app.wake_event)));
    }
}

/// `arg` as a count of threads, at least 1; anything else warns and keeps `fallback`.
std::size_t parse_threads(const char* arg, std::size_t fallback) {
    const std::string_view text(arg);
    std::size_t threads = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), threads);
    if (error != std::errc{} || end != text.data() + text.size() || threads == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "--threads \"%s\" is not a whole number from 1; using %zu",
                    arg, fallback);
        return fallback;
    }
    return threads;
}

/// `arg` as a seed: decimal digits only, in range. Anything else (letters, trailing
/// characters, a sign, too many digits) is warned about and starts on kDefaultSeed, so a
/// typo opens a game instead of ending the program.
Seed parse_seed(const char* arg) {
    const std::string_view text(arg);
    Seed seed = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seed);
    if (error != std::errc{} || end != text.data() + text.size()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "seed \"%s\" is not a whole number from 0 to %llu; starting on seed %llu", arg,
                    static_cast<unsigned long long>(std::numeric_limits<Seed>::max()),
                    static_cast<unsigned long long>(kDefaultSeed));
        return kDefaultSeed;
    }
    return seed;
}

} // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    auto* app = new App();
    *appstate = app;
    // Arguments: an optional seed, and `--threads N` anywhere (PEO-080).
    Seed seed = kDefaultSeed;
    std::size_t threads = default_threads();
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--threads") {
            if (i + 1 < argc) {
                threads = parse_threads(argv[++i], threads);
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "--threads needs a number; using %zu", threads);
            }
            continue;
        }
        seed = parse_seed(argv[i]);
    }
    SDL_Log("seed %llu, %zu thread(s) for the simulation", static_cast<unsigned long long>(seed), threads);
    app->pool.emplace(threads);
    app->world.emplace(seed, game_params());
    app->world->set_executor(&*app->pool);
    app->speculator.emplace(*app->world);
    app->speculator->request();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    const StageSpec& spec = app->world->stage().spec;
    const int win_w = spec.width * kCell * kScale;
    const int win_h = (spec.height + kHudRows) * kCell * kScale;
    if (!SDL_CreateWindowAndRenderer("Post-Event Onwards", win_w, win_h, 0, &app->window, &app->renderer)) {
        SDL_Log("CreateWindowAndRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    app->wake_event = SDL_RegisterEvents(1);
    const bool wait_event = SDL_GetVersion() >= kWaitEventMinVersion;
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, wait_event ? "waitevent" : kFallbackIterateHz);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<App*>(appstate);
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESIZED:
        app->dirty = true;
        break;
    case SDL_EVENT_KEY_DOWN: {
        const SDL_Keycode key = event->key.key;
        if (key == SDLK_ESCAPE) {
            app->input.clear();
            return SDL_APP_SUCCESS;
        }
        if (key == SDLK_S && (event->key.mod & SDL_KMOD_SHIFT)) {
            app->show_scent = !app->show_scent;
            app->dirty = true;
        } else if (key == SDLK_R) {
            app->running = !app->running; // a mode, not a turn: no action, no cap
            app->dirty = true;
        } else if (key == SDLK_N) {
            app->input.clear();               // taps were meant for the old stage
            (void)app->speculator->quiesce(); // the speculation is for the old stage
            app->world->load_stage(app->world->stage_index() + 1);
            app->speculator->request();
            app->dirty = true;
        } else if (const std::optional<Action> action = action_for(key, app->running)) {
            // Handling time, not event->key.timestamp: that is only as good as the
            // input device, and synthetic keyboards (wtype) stamp every press with
            // the same frozen time. Walk or run is fixed here, at press time.
            app->input.offer(*action, event->key.repeat, SDL_GetTicksNS());
            drain(*app);
        }
        break;
    }
    default:
        if (event->type == app->wake_event) {
            app->wake_timer = 0;
            drain(*app);
        }
        break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<App*>(appstate);
    if (app->dirty) {
        draw(*app);
        app->dirty = false;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) {
    auto* app = static_cast<App*>(appstate);
    if (app) {
        if (app->renderer) {
            SDL_DestroyRenderer(app->renderer);
        }
        if (app->window) {
            SDL_DestroyWindow(app->window);
        }
        delete app;
    }
}
