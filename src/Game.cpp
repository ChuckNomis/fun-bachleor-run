#include "Game.h"
#include "Components.h"
#include "EntityFactory.h"
#include "Systems.h"
#include <box2d/box2d.h>

static constexpr float LEVEL_W  = 5000.f;
// With CAMERA_ZOOM=1.5 the visible world height = 720/1.5 = 480 px.
// Ground top at y=370 → renders at 555 px (lower third, nice for a platformer).
static constexpr float GROUND_Y = 370.f;  // top surface of the ground strip (pixels)

// ── Helpers ──────────────────────────────────────────────────────────────────

static SDL_Texture* makeSolidTex(SDL_Renderer* r, Uint8 R, Uint8 G, Uint8 B)
{
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, 2, 2);
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderClear(r);
    SDL_SetRenderTarget(r, nullptr);
    return tex;
}

// ── Game ctor / dtor ─────────────────────────────────────────────────────────

Game::Game()
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer("Fun Run — Demo", SCREEN_W, SCREEN_H, 0, &_window, &_renderer);

    SDL_Surface* surf = IMG_Load("res/spritesheet.png");
    _spritesheet = surf ? SDL_CreateTextureFromSurface(_renderer, surf) : nullptr;
    SDL_DestroySurface(surf);
    if (_spritesheet)
        SDL_SetTextureBlendMode(_spritesheet, SDL_BLENDMODE_BLEND);

    _platTexture   = makeSolidTex(_renderer,  70, 130, 200);  // steel-blue platforms
    _groundTexture = makeSolidTex(_renderer, 100,  70,  40);  // brown dirt ground
    _startTex      = makeSolidTex(_renderer,  40, 210,  80);  // bright green start pole
    _finishTex     = makeSolidTex(_renderer, 220,  40,  40);  // red finish pole

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
    SDL_DestroyTexture(_platTexture);
    SDL_DestroyTexture(_groundTexture);
    SDL_DestroyTexture(_startTex);
    SDL_DestroyTexture(_finishTex);
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

// ── Level layout ─────────────────────────────────────────────────────────────

void Game::init_world()
{
    _camera = createCamera({ 0.f, 0.f });

    // ── Ground ───────────────────────────────────────────────────────────────
    // Ground body center is half-height below the surface.
    // At zoom=1.5, visible world height = 480 px.  GROUND_Y=370 renders at 555.
    constexpr float GH = 110.f;
    createPlatform(_physicsWorld, _groundTexture,
                   { LEVEL_W / 2.f, GROUND_Y + GH / 2.f }, LEVEL_W, GH);

    // Jump height from any surface ≈ 184 px (JUMP_VY=8.5, gravity=9.81, BOX_SCALE=50).
    // From GROUND_Y=370, max reachable platform top: 370−184 = 186.
    // Horizontal jump distance ≈ 216 px. Gap budget varies with height change.
    // All platform positions are body CENTERS.  Top surface = center.y − PH/2.
    constexpr float PH = 20.f;

    // ── Section 1: Easy steps – low platforms, small gaps (warm-up) ─────────
    //  Ground top=370; these tops are 40–85 px above ground → easy to reach.
    createPlatform(_physicsWorld, _platTexture, {  420.f, 335.f }, 200.f, PH); // top 325
    createPlatform(_physicsWorld, _platTexture, {  690.f, 318.f }, 180.f, PH); // top 308, gap≈80
    createPlatform(_physicsWorld, _platTexture, {  960.f, 305.f }, 180.f, PH); // top 295, gap≈80
    createPlatform(_physicsWorld, _platTexture, { 1220.f, 288.f }, 170.f, PH); // top 278, gap≈75

    // ── Section 2: Mid heights – proper jumps required ───────────────────────
    createPlatform(_physicsWorld, _platTexture, { 1470.f, 268.f }, 170.f, PH); // top 258, gap≈85
    createPlatform(_physicsWorld, _platTexture, { 1730.f, 252.f }, 160.f, PH); // top 242, gap≈90
    createPlatform(_physicsWorld, _platTexture, { 1990.f, 260.f }, 170.f, PH); // top 250, gap≈90
    createPlatform(_physicsWorld, _platTexture, { 2260.f, 242.f }, 160.f, PH); // top 232, gap≈95

    // ── Section 3: Wide platforms at altitude ────────────────────────────────
    createPlatform(_physicsWorld, _platTexture, { 2530.f, 250.f }, 240.f, PH); // top 240, gap≈110
    createPlatform(_physicsWorld, _platTexture, { 2840.f, 232.f }, 210.f, PH); // top 222, gap≈80
    createPlatform(_physicsWorld, _platTexture, { 3110.f, 245.f }, 200.f, PH); // top 235, gap≈55

    // ── Section 4: Precision – narrow platforms, 60–100 px gaps ─────────────
    createPlatform(_physicsWorld, _platTexture, { 3330.f, 262.f }, 130.f, PH); // top 252, gap≈65
    createPlatform(_physicsWorld, _platTexture, { 3510.f, 235.f }, 110.f, PH); // top 225, gap≈60  narrow!
    createPlatform(_physicsWorld, _platTexture, { 3680.f, 255.f }, 120.f, PH); // top 245, gap≈55
    createPlatform(_physicsWorld, _platTexture, { 3860.f, 228.f }, 110.f, PH); // top 218, gap≈70  narrow!
    createPlatform(_physicsWorld, _platTexture, { 4040.f, 250.f }, 140.f, PH); // top 240, gap≈60
    createPlatform(_physicsWorld, _platTexture, { 4230.f, 268.f }, 170.f, PH); // top 258, gap≈50

    // ── Section 5: Staircase back to ground ──────────────────────────────────
    createPlatform(_physicsWorld, _platTexture, { 4450.f, 298.f }, 190.f, PH); // top 288, gap≈55
    createPlatform(_physicsWorld, _platTexture, { 4660.f, 330.f }, 190.f, PH); // top 320, gap≈50

    // ── Start / Finish line decorations (visual only, no physics) ────────────
    // Pole: 10 px wide, fits from y=60 (top) down to ground surface at y=370.
    constexpr float POLE_W = 10.f;
    constexpr float POLE_H = GROUND_Y - 60.f;          // 310 px tall
    constexpr float POLE_CY = 60.f + POLE_H / 2.f;     // center of pole

    createDecoration(_startTex,  {  150.f, POLE_CY }, POLE_W, POLE_H);
    createDecoration(_finishTex, { LEVEL_W - 150.f, POLE_CY }, POLE_W, POLE_H);

    // ── Player ───────────────────────────────────────────────────────────────
    createPlayer(_physicsWorld, _spritesheet, { 200.f, 200.f }, 0);
}

// ── Main loop ────────────────────────────────────────────────────────────────

void Game::run()
{
    SystemContext ctx{
        _physicsWorld,
        _renderer,
        _spritesheet,
        _camera,
        1.f / TARGET_FPS,
        nullptr,      // no scrolling map texture
        LEVEL_W,
        static_cast<float>(SCREEN_H),
        CAMERA_ZOOM
    };

    Uint64 frameStart = SDL_GetTicks();

    while (_running) {
        SDL_Event events[64];
        int       eventCount = 0;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT ||
                (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_ESCAPE))
                _running = false;
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

        const Uint64 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < static_cast<Uint64>(GAME_FRAME_MS))
            SDL_Delay(static_cast<Uint32>(GAME_FRAME_MS - elapsed));
        frameStart += GAME_FRAME_MS;
    }
}
