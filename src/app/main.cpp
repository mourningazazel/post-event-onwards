// SDL3 frontend. This file owns the window, input and drawing; the simulation
// lives in peo::core and never sees SDL. Keep it thin: anything with logic in
// it belongs in core where it can be tested headlessly.

#include "peo/core/dead.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/scent.hpp"
#include "peo/core/stage.hpp"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace peo::core;

constexpr int kCell = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE; // 8 px glyphs
constexpr int kScale = 2;
constexpr int kHudRows = 1;
constexpr int kInitialHorde = 40;
constexpr float kPlayerScent = 1.0F;
constexpr Uint64 kTickMs = 100;

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    Seed world_seed = 1;
    std::uint32_t stage_index = 0;
    Stage stage;
    ScentField scent{1, 1};
    std::vector<Dead> horde;
    Vec2i player{};
    Rng rng{1};
    Tick tick = 0;
    Uint64 last_tick_ms = 0;
    bool paused = false;
    bool show_scent = false;
};

void load_stage(App& app, std::uint32_t index) {
    app.stage_index = index;
    app.stage = generate_stage(stage_spec(app.world_seed, index));
    app.scent = ScentField(app.stage.spec.width, app.stage.spec.height);
    app.player = app.stage.entry;
    app.rng.reseed(stage_seed(app.world_seed, index) ^ 0xABCDULL);
    app.horde.clear();
    for (int i = 0; i < kInitialHorde; ++i) {
        Vec2i p;
        do {
            p = {app.rng.range(1, app.stage.spec.width - 2), app.rng.range(1, app.stage.spec.height - 2)};
        } while (app.stage.blocked.at(p) || p == app.player);
        app.horde.push_back(
            {.pos = p, .cooldown = 0, .speed = static_cast<std::uint8_t>(app.rng.range(1, 3))});
    }
    app.tick = 0;
}

void try_move(App& app, Vec2i delta) {
    const Vec2i next = app.player + delta;
    if (app.stage.blocked.in_bounds(next) && !app.stage.blocked.at(next)) {
        app.player = next;
    }
    if (app.player == app.stage.exit) {
        load_stage(app, app.stage_index + 1);
    }
}

void simulate(App& app) {
    app.scent.deposit(app.player, kPlayerScent);
    app.scent.step(&app.stage.blocked);
    step_horde(app.horde, app.scent, app.stage.blocked);
    ++app.tick;
}

void draw(App& app) {
    SDL_SetRenderDrawColor(app.renderer, 8, 8, 12, 255);
    SDL_RenderClear(app.renderer);
    SDL_SetRenderScale(app.renderer, kScale, kScale);

    const int w = app.stage.spec.width;
    const int h = app.stage.spec.height;
    std::string row(static_cast<std::size_t>(w), ' ');

    // Map + optional scent heat.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            char c = app.stage.blocked.at(x, y) ? '#' : '.';
            if (app.show_scent && !app.stage.blocked.at(x, y)) {
                const float s = app.scent.sample({x, y});
                c = s > 0.5F ? '@' : s > 0.1F ? '+' : s > 0.01F ? ':' : '.';
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
    glyph(app.stage.exit, ">", 120, 200, 255);
    for (const Dead& d : app.horde) {
        glyph(d.pos, "d", 220, 60, 60);
    }
    glyph(app.player, "@", 255, 255, 255);

    char hud[128];
    std::snprintf(hud, sizeof hud,
                  "stage %u  tick %llu  dead %zu  [arrows/wasd move] [space pause] [s scent] %s",
                  app.stage_index, static_cast<unsigned long long>(app.tick), app.horde.size(),
                  app.paused ? "PAUSED" : "");
    SDL_SetRenderDrawColor(app.renderer, 200, 200, 120, 255);
    SDL_RenderDebugText(app.renderer, 0.0F, 0.0F, hud);

    SDL_SetRenderScale(app.renderer, 1.0F, 1.0F);
    SDL_RenderPresent(app.renderer);
}

} // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    auto* app = new App();
    *appstate = app;
    if (argc > 1) {
        app->world_seed = std::stoull(argv[1]);
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    load_stage(*app, 0);
    const int win_w = app->stage.spec.width * kCell * kScale;
    const int win_h = (app->stage.spec.height + kHudRows) * kCell * kScale;
    if (!SDL_CreateWindowAndRenderer("Post-Event Onwards", win_w, win_h, 0, &app->window, &app->renderer)) {
        SDL_Log("CreateWindowAndRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    app->last_tick_ms = SDL_GetTicks();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<App*>(appstate);
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.key) {
        case SDLK_ESCAPE:
            return SDL_APP_SUCCESS;
        case SDLK_UP:
        case SDLK_W:
            try_move(*app, {0, -1});
            break;
        case SDLK_DOWN:
        case SDLK_S:
            if (event->key.key == SDLK_S && (event->key.mod & SDL_KMOD_SHIFT)) {
                app->show_scent = !app->show_scent;
            } else {
                try_move(*app, {0, 1});
            }
            break;
        case SDLK_LEFT:
        case SDLK_A:
            try_move(*app, {-1, 0});
            break;
        case SDLK_RIGHT:
        case SDLK_D:
            try_move(*app, {1, 0});
            break;
        case SDLK_SPACE:
            app->paused = !app->paused;
            break;
        case SDLK_N:
            load_stage(*app, app->stage_index + 1);
            break;
        default:
            break;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<App*>(appstate);
    const Uint64 now = SDL_GetTicks();
    while (!app->paused && now - app->last_tick_ms >= kTickMs) {
        simulate(*app);
        app->last_tick_ms += kTickMs;
    }
    if (app->paused) {
        app->last_tick_ms = now;
    }
    draw(*app);
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
