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
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

#include "camera.hpp"
#include "glyph_atlas.hpp"
#include "thread_pool.hpp"
#ifdef PEO_HAVE_GPU
#include "gpu_field.hpp"
#endif

namespace {

using namespace peo::core;
using peo::app::atlas_cell;
using peo::app::atlas_glyph_px;
using peo::app::AtlasCell;
using peo::app::display_scale;
using peo::app::layout_view;
using peo::app::View;
using peo::app::view_origin;
using peo::app::view_to_screen;
using peo::app::ViewLayout;

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

/// The stage size, in cells, from which the speculation's scent update runs on the GPU
/// (PEO-081). Measured on the M1 (Honeykrisp), saturated field, readback included: the
/// GPU takes 0.8 / 2.0-2.5 / 8.8-10.2 ms at 512, 1024 and 2048 square, about one CPU
/// thread (0.6 / 2.4 / 10.1 ms) and never 4 workers (0.25 / 0.9 / 4.0 ms), so by default
/// it never runs.
/// `--gpu-min-cells N` opts in for a machine where it does; `--no-gpu-compute` forbids it.
constexpr std::size_t kGpuFieldMinCells = std::numeric_limits<std::size_t>::max();
/// Under a wind with gusts (PEO-087) the CPU's windy rounds cost about five calm ones and
/// the GPU wins from 512x512: 0.9 against 1.3 ms on 4 CPU workers there, 2.6 against 6.6 at
/// 1024, 9.8 against 28 at 2048 (full wind, gust 2, readback included); 256x256 is 0.53
/// against 0.34 ms.
constexpr std::size_t kGpuWindyMinCells = std::size_t{512} * 512;

#ifdef PEO_HAVE_GPU
/// Owns the compute device; declared before the backend, so destroyed after it.
struct GpuDevice {
    SDL_GPUDevice* device = nullptr;
    GpuDevice() = default;
    GpuDevice(const GpuDevice&) = delete;
    GpuDevice& operator=(const GpuDevice&) = delete;
    ~GpuDevice() {
        if (device != nullptr) {
            SDL_DestroyGPUDevice(device);
        }
    }
};
#endif

/// Threads that run core's pieces (PEO-080): the hardware's, less one for the main
/// thread, at least one. `--threads N` overrides it; 1 is serial.
std::size_t default_threads() noexcept {
    const unsigned hardware = std::thread::hardware_concurrency();
    return hardware > 1 ? hardware - 1 : 1;
}

/// The game's stage (PEO-117): bigger than the window, which shows a view of it centred
/// on the player. `--stage WxH` chooses another, each side in [kMinGameStageSide,
/// kMaxGameStageSide] (the PEO-083 ceiling).
constexpr int kGameStageWidth = 200;
constexpr int kGameStageHeight = 120;
constexpr int kMinGameStageSide = 16;
constexpr int kMaxGameStageSide = 2048;
/// The Dead on the old 80x45 default stage; other stages keep that density, so
/// `--stage 80x45` plays as the game did before PEO-117.
constexpr int kDefaultStageDead = 40;
constexpr int kDensityStageWidth = 80;
constexpr int kDensityStageHeight = 45;

WorldParams game_params(int stage_width, int stage_height) noexcept {
    WorldParams params;
    params.wind_max = kGameWindMax;
    params.scent.gust = kGameWindGust;
    params.stage_width = stage_width;
    params.stage_height = stage_height;
    // Worst case 40 x 2048 x 2048 = 1.7e8 (46,603 Dead; World clamps to the open cells).
    const auto dead = std::int64_t{kDefaultStageDead} * stage_width * stage_height /
                      (std::int64_t{kDensityStageWidth} * kDensityStageHeight);
    params.initial_dead = static_cast<int>(dead);
    return params;
}
constexpr int kScale = 2;
constexpr int kHudRows = 2; // a status line and a key-hint line (PEO-062)
/// DF-grid tilesets mark their background magenta instead of alpha (PEO-097); keying it out
/// keeps those drop-in tilesets from drawing magenta boxes.
constexpr SDL_Color kAtlasColourKey{255, 0, 255, SDL_ALPHA_OPAQUE};
/// The window opens on this many map cells (today's size on the old default stage) and
/// cannot shrink below the minimum: the HUD lines are written to fit 80 columns, and 21
/// map rows (PEO-117's brief) still show ten cells either side of the player.
constexpr int kDefaultViewCols = 80;
constexpr int kDefaultViewRows = 45;
constexpr int kMinViewCols = 80;
constexpr int kMinViewRows = 21;
/// SDL_AppIterate pacing. Nothing changes between key presses, so sleep until an
/// event arrives ("waitevent", SDL 3.4+). Older SDL parses that string as 0 and
/// would spin, so there it gets a frame cap instead. The SDL fetched from source
/// (cmake/FindOrFetchSDL3.cmake) is 3.4, so the cap only applies when find_package
/// picks up an older installed SDL.
constexpr int kWaitEventMinVersion = SDL_VERSIONNUM(3, 4, 0);
constexpr const char* kFallbackIterateHz = "60";

/// The status line prints speculate time as whole ms and a three-digit us fraction (PEO-104).
constexpr std::uint64_t kMicrosPerMilli = 1000;
/// The status line's buffer: the format with every number at its widest (several 20-digit
/// counts) is 167 bytes with its terminator.
constexpr std::size_t kStatusLineBytes = 192;

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
/// never touch it at once. The buffer is reused, so a turn allocates nothing. It
/// also times each run (PEO-104): wall time is the app's to measure, never core's.
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

    /// How long the last finished speculate() took, in microseconds; 0 until one finishes.
    [[nodiscard]] std::uint64_t last_speculate_us() {
        const std::lock_guard lock(mutex_);
        return last_us_;
    }

private:
    void run(std::stop_token st) {
        std::unique_lock lock(mutex_);
        while (wake_.wait(lock, st, [this] { return requested_; })) {
            requested_ = false;
            busy_ = true;
            lock.unlock();
            const auto start = std::chrono::steady_clock::now();
            world_->speculate(buffer_);
            const auto took = std::chrono::steady_clock::now() - start;
            lock.lock();
            last_us_ = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(took).count());
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
    std::uint64_t last_us_ = 0;
    Speculation buffer_;
    std::jthread worker_; // last: joins before the state above is destroyed
};

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
#ifdef PEO_HAVE_GPU
    /// The scent's GPU backend (PEO-081) and its device: before world and speculator, so
    /// destroyed after the speculation worker has stopped using them.
    GpuDevice gpu_device;
    std::optional<peo::gpu::GpuFieldBackend> gpu;
#endif
    /// Core's executor (D-035): before world and speculator, so destroyed after them.
    std::optional<peo::app::ThreadPool> pool;
    std::optional<World> world;
    std::optional<Speculator> speculator; // after world: destroyed (joined) first
    bool show_scent = false;
    /// The DF-grid tileset every glyph is drawn from (PEO-097), or null in ASCII mode (the
    /// debug font). SDL_AppQuit destroys it before the renderer.
    SDL_Texture* atlas = nullptr;
    /// One glyph's side in the atlas, in its own pixels.
    int glyph_px = kCell;
    /// The HUD's cell in window pixels: the glyph at its display scale (ADR-0003).
    int hud_cell_px = kCell * kScale;
    /// The map layer's cell in window pixels: equal to the HUD's until zoom (D-050), which
    /// changes only this.
    int map_cell_px = kCell * kScale;
    /// One map row of glyphs, reused across draws; grown only when the view widens.
    std::string row;
    /// R toggles it (D-015): steps take kRunStepSubsteps. Kept across stages.
    bool running = false;
    /// Redraw only when something changed (a turn, a view toggle, an expose).
    bool dirty = true;
    /// PEO-007 manual test: was the last turn's speculation ready at input?
    bool last_hit = false;
    unsigned long long misses = 0;
    /// PEO-104: the speculate() this turn's hit refers to, or on a miss the last one to
    /// finish, in microseconds.
    std::uint64_t last_spec_us = 0;
    /// Turn keys to turns (D-032): taps wait up to 3 deep, played out at 3 a second.
    TurnInput input;
    /// A registered SDL event type the wake timer pushes, and the pending timer (0: none).
    Uint32 wake_event = 0;
    SDL_TimerID wake_timer = 0;
};

/// The HUD's height in window pixels.
int hud_px(const App& app) noexcept {
    return kHudRows * app.hud_cell_px;
}

/// Back to the debug font's cells (ASCII mode), as before PEO-097.
void use_ascii_cells(App& app) noexcept {
    app.glyph_px = kCell;
    app.hud_cell_px = kCell * kScale;
    app.map_cell_px = app.hud_cell_px;
}

/// The window size that shows `cols` by `rows` map cells and the HUD, in window pixels.
SDL_Point window_px(const App& app, int cols, int rows) noexcept {
    return {cols * app.map_cell_px, rows * app.map_cell_px + hud_px(app)};
}

/// Draw `text` from window pixel (`x_px`, `y_px`), one glyph per `cell_px`, in one colour.
/// Tileset mode copies each non-space byte's atlas glyph; ASCII mode is the debug font
/// under draw()'s render scale of kScale, whose glyphs advance kCell, so `cell_px` there
/// is always kCell * kScale.
void draw_text(App& app, float x_px, float y_px, float cell_px, const char* text, Uint8 r, Uint8 g, Uint8 b) {
    if (app.atlas == nullptr) {
        SDL_SetRenderDrawColor(app.renderer, r, g, b, SDL_ALPHA_OPAQUE);
        SDL_RenderDebugText(app.renderer, x_px / kScale, y_px / kScale, text);
        return;
    }
    SDL_SetTextureColorMod(app.atlas, r, g, b);
    const auto glyph = static_cast<float>(app.glyph_px);
    for (const char* c = text; *c != '\0'; ++c, x_px += cell_px) {
        if (*c == ' ') {
            continue;
        }
        const AtlasCell at = atlas_cell(static_cast<unsigned char>(*c));
        const SDL_FRect src{static_cast<float>(at.col) * glyph, static_cast<float>(at.row) * glyph, glyph,
                            glyph};
        const SDL_FRect dst{x_px, y_px, cell_px, cell_px};
        SDL_RenderTexture(app.renderer, app.atlas, &src, &dst);
    }
}

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
    app.last_spec_us = app.speculator->last_speculate_us(); // the worker is idle after quiesce()
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
    // The view (PEO-117): whole map cells of the window below the HUD, centred on the
    // player. Laid out and drawn in window pixels; ASCII mode's debug font draws under a
    // render scale of kScale (draw_text divides by it), a tileset at 1.
    int out_w = 0;
    int out_h = 0;
    SDL_GetCurrentRenderOutputSize(app.renderer, &out_w, &out_h);
    const float render_scale = app.atlas == nullptr ? static_cast<float>(kScale) : 1.0F;
    SDL_SetRenderScale(app.renderer, render_scale, render_scale);
    const ViewLayout layout = layout_view(out_w, out_h, app.map_cell_px, hud_px(app));
    const View view = layout.view;
    const Vec2i origin = view_origin(world.player(), view);
    const auto cell = static_cast<float>(app.map_cell_px);
    const auto left = static_cast<float>(layout.left_px);
    const auto top = static_cast<float>(layout.top_px + hud_px(app));
    if (app.row.size() < static_cast<std::size_t>(view.cols) + 1) {
        app.row.resize(static_cast<std::size_t>(view.cols) + 1); // the glyphs and a '\0'
    }
    std::string& row = app.row;
    const auto in_stage = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < stage.spec.width && y < stage.spec.height;
    };

    // Map + optional scent heat over the view's in-stage cells, so the bands read relative
    // to what is on screen; cells beyond the stage stay dark. @ is drawn last, on top.
    const ScentWave& scent = world.scent();
    std::int32_t max_scent = 0;
    for (int vy = 0; app.show_scent && vy < view.rows; ++vy) {
        for (int vx = 0; vx < view.cols; ++vx) {
            if (in_stage(origin.x + vx, origin.y + vy)) {
                max_scent = std::max(max_scent, scent.sample({origin.x + vx, origin.y + vy}));
            }
        }
    }
    for (int vy = 0; vy < view.rows; ++vy) {
        const int y = origin.y + vy;
        for (int vx = 0; vx < view.cols; ++vx) {
            const int x = origin.x + vx;
            char c = ' ';
            if (in_stage(x, y)) {
                c = stage.blocked.at(x, y) ? '#' : '.';
                if (app.show_scent && !stage.blocked.at(x, y)) {
                    c = scent_glyph(scent.sample({x, y}), max_scent);
                }
            }
            row[static_cast<std::size_t>(vx)] = c;
        }
        row[static_cast<std::size_t>(view.cols)] = '\0';
        draw_text(app, left, top + static_cast<float>(vy) * cell, cell, row.c_str(), 90, 90, 100);
    }

    auto glyph = [&](Vec2i p, const char* s, Uint8 r, Uint8 g, Uint8 b) {
        const std::optional<Vec2i> at = view_to_screen(p, origin, view);
        if (!at) {
            return;
        }
        draw_text(app, left + static_cast<float>(at->x) * cell, top + static_cast<float>(at->y) * cell, cell,
                  s, r, g, b);
    };
    glyph(stage.exit, ">", 120, 200, 255);
    for (const Dead& d : world.horde()) {
        glyph(d.pos, "d", 220, 60, 60);
    }
    glyph(world.player(), "@", 255, 255, 255);

    // Two lines, each under the window's minimum 80 columns (kMinViewCols). The wind is named by where
    // it blows to, so "to E" is downwind east.
    const Wind wind = world.wind();
    const auto point = static_cast<std::size_t>(((wind.toward_degrees + kCompassStep / 2) / kCompassStep) %
                                                static_cast<std::int32_t>(kCompass.size()));
    // Sized for every field at its widest, so snprintf never truncates; real lines stay under 80.
    char status[kStatusLineBytes];
    std::snprintf(status, sizeof status,
                  "stage %u  turn %llu  %s  dead %zu  wind to %s %d  spec:%s %llu.%03llums (miss %llu)",
                  world.stage_index(), static_cast<unsigned long long>(world.turn()),
                  app.running ? "run" : "walk", world.horde().size(),
                  wind.intensity > 0 ? kCompass[point] : "-", wind.intensity, app.last_hit ? "hit" : "miss",
                  static_cast<unsigned long long>(app.last_spec_us / kMicrosPerMilli),
                  static_cast<unsigned long long>(app.last_spec_us % kMicrosPerMilli), app.misses);
    static constexpr const char* kKeyHints =
        "[arrows/wasd] move [space/.] wait [r] run [shift+s] scent [n] next";
    const auto hud_cell = static_cast<float>(app.hud_cell_px);
    draw_text(app, 0.0F, 0.0F, hud_cell, status, 200, 200, 120);
    draw_text(app, 0.0F, hud_cell, hud_cell, kKeyHints, 140, 140, 100);

    SDL_SetRenderScale(app.renderer, 1.0F, 1.0F);
    SDL_RenderPresent(app.renderer);
}

/// Map a key to the action it spends a turn on, if any. Running shortens steps
/// only; a wait lasts kWaitSubsteps either way.
std::optional<Action> action_for(SDL_Keycode key, bool running) {
    const Substeps step = running ? kRunStepSubsteps : kStepSubsteps;
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

/// `arg`, the value of option `option`, as a count of at least 1; anything else warns and
/// keeps `fallback`.
std::size_t parse_count(const char* option, const char* arg, std::size_t fallback) {
    const std::string_view text(arg);
    std::size_t count = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), count);
    if (error != std::errc{} || end != text.data() + text.size() || count == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s \"%s\" is not a whole number from 1; keeping %zu",
                    option, arg, fallback);
        return fallback;
    }
    return count;
}

/// `--stage WxH` (PEO-117): two whole numbers around an 'x', each in [kMinGameStageSide,
/// kMaxGameStageSide]. Anything else is warned about and keeps the game's stage.
std::optional<Vec2i> parse_stage(const char* arg) {
    const std::string_view text(arg);
    const std::size_t cross = text.find('x');
    const auto side = [](std::string_view part) -> std::optional<int> {
        int value = 0;
        const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
        if (part.empty() || error != std::errc{} || end != part.data() + part.size() ||
            value < kMinGameStageSide || value > kMaxGameStageSide) {
            return std::nullopt;
        }
        return value;
    };
    if (cross != std::string_view::npos) {
        const std::optional<int> w = side(text.substr(0, cross));
        const std::optional<int> h = side(text.substr(cross + 1));
        if (w && h) {
            return Vec2i{*w, *h};
        }
    }
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "--stage \"%s\" is not WxH with each side from %d to %d; keeping %dx%d", arg,
                kMinGameStageSide, kMaxGameStageSide, kGameStageWidth, kGameStageHeight);
    return std::nullopt;
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

struct SurfaceDeleter {
    void operator()(SDL_Surface* surface) const noexcept { SDL_DestroySurface(surface); }
};
using SurfacePtr = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

/// `--tileset PATH` (PEO-097): the PNG at `path` if it is a DF-grid atlas, else null after a
/// warning that names why, and the game stays in ASCII mode.
SurfacePtr load_atlas(const char* path) {
#if SDL_VERSION_ATLEAST(3, 4, 0)
    SurfacePtr surface(SDL_LoadPNG(path));
    if (!surface) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "--tileset \"%s\" is unreadable or not a PNG (%s); ASCII mode", path, SDL_GetError());
        return nullptr;
    }
    if (!atlas_glyph_px(surface->w, surface->h)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "--tileset \"%s\" is %dx%d; it needs a square atlas whose side divides by %d; ASCII mode",
                    path, surface->w, surface->h, peo::app::kAtlasGlyphsPerSide);
        return nullptr;
    }
    return surface;
#else
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "--tileset \"%s\" needs SDL 3.4 or later (built with %d.%d.%d); ASCII mode", path,
                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    return nullptr;
#endif
}

} // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    auto* app = new App();
    *appstate = app;
    // Arguments: an optional seed, and anywhere `--threads N` (PEO-080),
    // `--no-gpu-compute` and `--gpu-min-cells N` (PEO-081), `--stage WxH` (PEO-117),
    // `--tileset PATH` (PEO-097).
    Seed seed = kDefaultSeed;
    Vec2i stage_size{kGameStageWidth, kGameStageHeight};
    std::size_t threads = default_threads();
    bool gpu_compute = true;
    std::size_t gpu_min_cells = kGpuFieldMinCells;
    bool gpu_min_given = false;
    const char* tileset = nullptr;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--threads" || arg == "--gpu-min-cells") {
            std::size_t& value = arg == "--threads" ? threads : gpu_min_cells;
            gpu_min_given = gpu_min_given || arg == "--gpu-min-cells";
            if (i + 1 < argc) {
                value = parse_count(argv[i], argv[i + 1], value);
                ++i;
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s needs a number", argv[i]);
            }
            continue;
        }
        if (arg == "--stage") {
            if (i + 1 < argc) {
                stage_size = parse_stage(argv[i + 1]).value_or(stage_size);
                ++i;
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "--stage needs WxH");
            }
            continue;
        }
        if (arg == "--tileset") {
            if (i + 1 < argc) {
                tileset = argv[i + 1];
                ++i;
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "--tileset needs a PATH");
            }
            continue;
        }
        if (arg == "--no-gpu-compute") {
            gpu_compute = false;
            continue;
        }
        seed = parse_seed(argv[i]);
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("seed %llu, %zu thread(s) for the simulation", static_cast<unsigned long long>(seed), threads);
    app->pool.emplace(threads);
    const WorldParams params = game_params(stage_size.x, stage_size.y);
    SDL_Log("stage %dx%d with %d Dead", params.stage_width, params.stage_height, params.initial_dead);
    app->world.emplace(seed, params);
    app->world->set_executor(&*app->pool);
#ifdef PEO_HAVE_GPU
    // A device only when the GPU could run on this stage; without one the CPU runs, as
    // before. --gpu-min-cells sets both thresholds.
    const std::size_t windy_min_cells = gpu_min_given ? gpu_min_cells : kGpuWindyMinCells;
    const StageSpec& stage = app->world->stage().spec;
    const auto stage_cells = static_cast<std::size_t>(stage.width) * static_cast<std::size_t>(stage.height);
    if (gpu_compute && stage_cells >= std::min(gpu_min_cells, windy_min_cells)) {
        app->gpu_device.device = peo::gpu::create_compute_device();
        if (app->gpu_device.device != nullptr) {
            app->gpu.emplace(app->gpu_device.device, gpu_min_cells, windy_min_cells);
        }
    }
    if (app->gpu && app->gpu->ready()) {
        app->world->set_field_backend(&*app->gpu);
        SDL_Log("GPU scent field from %zu cells calm, %zu windy (%s)", gpu_min_cells, windy_min_cells,
                SDL_GetGPUDeviceDriver(app->gpu_device.device));
    } else {
        SDL_Log("scent field on the CPU%s", gpu_compute ? "" : " (--no-gpu-compute)");
    }
#else
    (void)gpu_compute;
    (void)gpu_min_cells;
    SDL_Log("scent field on the CPU (built without peo_gpu)");
#endif
    app->speculator.emplace(*app->world);
    app->speculator->request();
    // A tileset sets the one cell size (ADR-0003) before the window is sized from it.
    SurfacePtr atlas = tileset != nullptr ? load_atlas(tileset) : nullptr;
    if (atlas) {
        app->glyph_px = atlas_glyph_px(atlas->w, atlas->h).value_or(kCell);
        app->hud_cell_px = app->glyph_px * display_scale(app->glyph_px);
        app->map_cell_px = app->hud_cell_px;
    }
    // The window shows a view of the stage (PEO-117), so its size no longer comes from it.
    const SDL_Point win = window_px(*app, kDefaultViewCols, kDefaultViewRows);
    if (!SDL_CreateWindowAndRenderer("Post-Event Onwards", win.x, win.y, SDL_WINDOW_RESIZABLE, &app->window,
                                     &app->renderer)) {
        SDL_Log("CreateWindowAndRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (atlas) {
        SDL_SetSurfaceColorKey(
            atlas.get(), true,
            SDL_MapSurfaceRGB(atlas.get(), kAtlasColourKey.r, kAtlasColourKey.g, kAtlasColourKey.b));
        app->atlas = SDL_CreateTextureFromSurface(app->renderer, atlas.get());
        atlas.reset();
        if (app->atlas != nullptr) {
            SDL_SetTextureScaleMode(app->atlas, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(app->atlas, SDL_BLENDMODE_BLEND);
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "--tileset \"%s\" made no texture (%s); ASCII mode",
                        tileset, SDL_GetError());
            use_ascii_cells(*app);
            const SDL_Point ascii = window_px(*app, kDefaultViewCols, kDefaultViewRows);
            SDL_SetWindowSize(app->window, ascii.x, ascii.y);
        }
    }
    const SDL_Point min = window_px(*app, kMinViewCols, kMinViewRows);
    SDL_SetWindowMinimumSize(app->window, min.x, min.y);
    if (app->atlas != nullptr) {
        SDL_Log("tileset %s, %d px glyphs at %dx", tileset, app->glyph_px, display_scale(app->glyph_px));
    } else {
        SDL_Log("ASCII mode");
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
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: // the view is laid out in output pixels
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
        if (app->atlas) {
            SDL_DestroyTexture(app->atlas);
        }
        if (app->renderer) {
            SDL_DestroyRenderer(app->renderer);
        }
        if (app->window) {
            SDL_DestroyWindow(app->window);
        }
        delete app;
    }
}
