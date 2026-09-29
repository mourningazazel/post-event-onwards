// SDL3 frontend. This file owns the window, input and drawing; the simulation
// lives in peo::core::World and never sees SDL. Keep it thin: anything with
// logic in it belongs in core where it can be tested headlessly.
//
// Turn-based (D-002): a key press becomes exactly one World::step(Action);
// nothing advances on a timer.

#include "peo/core/world.hpp"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>

namespace {

using namespace peo::core;

constexpr int kCell = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE; // 8 px glyphs
constexpr int kScale = 2;
constexpr int kHudRows = 1;
/// SDL_AppIterate pacing. Nothing changes between key presses, so sleep until an
/// event arrives ("waitevent", SDL 3.4+). Older SDL parses that string as 0 and
/// would spin, so there it gets a frame cap instead.
constexpr int kWaitEventMinVersion = SDL_VERSIONNUM(3, 4, 0);
constexpr const char* kFallbackIterateHz = "60";

/// Scent view bands, log-spaced relative to the field's current maximum so the
/// view reads the same at any deposit scale (0-500 today, D-007). Strongest first;
/// none uses @, which is the player's glyph.
struct ScentBand {
    float fraction_of_max;
    char glyph;
};
constexpr std::array<ScentBand, 3> kScentBands{{{1e-1F, '*'}, {1e-3F, '+'}, {1e-5F, ':'}}};

char scent_glyph(float scent, float max_scent) {
    for (const ScentBand& band : kScentBands) {
        if (scent > 0.0F && scent >= band.fraction_of_max * max_scent) {
            return band.glyph;
        }
    }
    return '.';
}

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::optional<World> world;
    bool show_scent = false;
    /// Redraw only when something changed (a turn, a view toggle, an expose).
    bool dirty = true;
};

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
    const Grid<float>& scent = world.scent().cells();
    const float max_scent = app.show_scent ? *std::max_element(scent.begin(), scent.end()) : 0.0F;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            char c = stage.blocked.at(x, y) ? '#' : '.';
            if (app.show_scent && !stage.blocked.at(x, y)) {
                c = scent_glyph(scent.at(x, y), max_scent);
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

    char hud[128];
    std::snprintf(hud, sizeof hud,
                  "stage %u  turn %llu  dead %zu  [arrows/wasd move] [space/. wait] [shift+s scent] [n next]",
                  world.stage_index(), static_cast<unsigned long long>(world.turn()), world.horde().size());
    SDL_SetRenderDrawColor(app.renderer, 200, 200, 120, 255);
    SDL_RenderDebugText(app.renderer, 0.0F, 0.0F, hud);

    SDL_SetRenderScale(app.renderer, 1.0F, 1.0F);
    SDL_RenderPresent(app.renderer);
}

/// Map a key to the action it spends a turn on, if any.
std::optional<Action> action_for(SDL_Keycode key) {
    switch (key) {
    case SDLK_UP:
    case SDLK_W:
        return Action::step({0, -1});
    case SDLK_DOWN:
    case SDLK_S:
        return Action::step({0, 1});
    case SDLK_LEFT:
    case SDLK_A:
        return Action::step({-1, 0});
    case SDLK_RIGHT:
    case SDLK_D:
        return Action::step({1, 0});
    case SDLK_SPACE:
    case SDLK_PERIOD:
        return Action::wait();
    default:
        return std::nullopt;
    }
}

} // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    auto* app = new App();
    *appstate = app;
    Seed seed = 1;
    if (argc > 1) {
        seed = std::stoull(argv[1]);
    }
    app->world.emplace(seed);

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
            return SDL_APP_SUCCESS;
        }
        if (key == SDLK_S && (event->key.mod & SDL_KMOD_SHIFT)) {
            app->show_scent = !app->show_scent;
            app->dirty = true;
        } else if (key == SDLK_N) {
            app->world->load_stage(app->world->stage_index() + 1);
            app->dirty = true;
        } else if (const std::optional<Action> action = action_for(key)) {
            app->world->step(*action); // exactly one turn per key press
            app->dirty = true;
        }
        break;
    }
    default:
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
