#include "Game.h"
#include "Components.h"
#include "EntityFactory.h"
#include "Systems.h"

Game::Game()
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer("Fun Run", SCREEN_W, SCREEN_H, 0, &_window, &_renderer);

    SDL_Surface* surf = IMG_Load("res/spritesheet.png");
    _spritesheet = SDL_CreateTextureFromSurface(_renderer, surf);
    SDL_DestroySurface(surf);

    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity    = { 0.f, 9.81f };
    _physicsWorld       = b2CreateWorld(&worldDef);

    init_world();
    _running = true;
}

Game::~Game()
{
    b2DestroyWorld(_physicsWorld);
    SDL_DestroyTexture(_spritesheet);
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

void Game::init_world()
{
    _camera = createCamera({ 0.f, 0.f });

    // TODO: load the actual level layout from map.png / level data.
    // For the scaffold, spawn a ground platform and two test players.
    createPlatform(_physicsWorld, _spritesheet, { SCREEN_W / 2.f, 680.f }, 1280.f, 32.f);

    createPlayer(_physicsWorld, _spritesheet, { 200.f, 600.f }, 0);
    createPlayer(_physicsWorld, _spritesheet, { 400.f, 600.f }, 1);
}

void Game::run()
{
    SystemContext ctx{
        _physicsWorld,
        _renderer,
        _spritesheet,
        _camera,
        1.f / TARGET_FPS
    };

    Uint64 frameStart = SDL_GetTicks();

    while (_running) {
        SDL_Event events[64];
        int       eventCount = 0;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT ||
                (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_ESCAPE)) {
                _running = false;
            }
            if (eventCount < 64)
                events[eventCount++] = ev;
        }

        input_system(events, eventCount);
        controller_system(ctx);
        physics_system(ctx);
        sensor_system(ctx);
        damage_system(ctx);
        camera_system(ctx);
        render_system(ctx);

        const Uint64 frameEnd = SDL_GetTicks();
        const Uint64 elapsed  = frameEnd - frameStart;
        if (elapsed < static_cast<Uint64>(GAME_FRAME_MS))
            SDL_Delay(static_cast<Uint32>(GAME_FRAME_MS - elapsed));
        frameStart += GAME_FRAME_MS;
    }
}
