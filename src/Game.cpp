#include "Game.h"
#include "Components.h"
#include "EntityFactory.h"
#include "LevelBuilder.h"
#include "Systems.h"
#include "TileConfig.h"
#include <box2d/box2d.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <vector>

static bool isNeutralBackground(Uint8 r, Uint8 g, Uint8 b)
{
    const int maxC = std::max(int(r), std::max(int(g), int(b)));
    const int minC = std::min(int(r), std::min(int(g), int(b)));
    return (maxC - minC) <= 15 && maxC >= 168;
}

static bool isNearWhiteBackground(Uint8 r, Uint8 g, Uint8 b)
{
    return r >= 235 && g >= 235 && b >= 235;
}

static void stripBackground(SDL_Surface* surf, bool (*isBackground)(Uint8, Uint8, Uint8))
{
    if (!surf)
        return;

    const int w = surf->w;
    const int h = surf->h;
    std::vector<uint8_t> visited(static_cast<std::size_t>(w * h), 0);
    std::queue<std::pair<int, int>> q;

    auto trySeed = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h)
            return;
        const std::size_t idx = static_cast<std::size_t>(y * w + x);
        if (visited[idx])
            return;
        Uint8 r = 0, g = 0, b = 0, a = 0;
        if (!SDL_ReadSurfacePixel(surf, x, y, &r, &g, &b, &a))
            return;
        if (!isBackground(r, g, b))
            return;
        visited[idx] = 1;
        q.emplace(x, y);
    };

    for (int x = 0; x < w; ++x) {
        trySeed(x, 0);
        trySeed(x, h - 1);
    }
    for (int y = 0; y < h; ++y) {
        trySeed(0, y);
        trySeed(w - 1, y);
    }

    while (!q.empty()) {
        const int x = q.front().first;
        const int y = q.front().second;
        q.pop();

        Uint8 r = 0, g = 0, b = 0, a = 0;
        if (SDL_ReadSurfacePixel(surf, x, y, &r, &g, &b, &a))
            SDL_WriteSurfacePixel(surf, x, y, r, g, b, 0);

        trySeed(x + 1, y);
        trySeed(x - 1, y);
        trySeed(x, y + 1);
        trySeed(x, y - 1);
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Uint8 r = 0, g = 0, b = 0, a = 0;
            if (!SDL_ReadSurfacePixel(surf, x, y, &r, &g, &b, &a))
                continue;
            if (a > 0)
                SDL_WriteSurfacePixel(surf, x, y, r, g, b, 255);
        }
    }
}

static void stripCheckerboardBackground(SDL_Surface* surf)
{
    stripBackground(surf, isNeutralBackground);
}

static void stripWhiteBackground(SDL_Surface* surf)
{
    stripBackground(surf, isNearWhiteBackground);
}

static void keyOutBlackPixels(SDL_Surface* surf)
{
    if (!surf)
        return;

    for (int y = 0; y < surf->h; ++y) {
        for (int x = 0; x < surf->w; ++x) {
            Uint8 r = 0, g = 0, b = 0, a = 0;
            if (!SDL_ReadSurfacePixel(surf, x, y, &r, &g, &b, &a))
                continue;
            if (r <= 12 && g <= 12 && b <= 12)
                SDL_WriteSurfacePixel(surf, x, y, r, g, b, 0);
            else
                SDL_WriteSurfacePixel(surf, x, y, r, g, b, 255);
        }
    }
}

static SDL_Texture* loadTerrainTexture(SDL_Renderer* renderer, const char* path)
{
    SDL_Surface* loaded = IMG_Load(path);
    if (!loaded)
        return nullptr;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, loaded);
    SDL_DestroySurface(loaded);
    if (tex)
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

static SDL_Texture* cropTile(SDL_Renderer* renderer, const char* path, int sx, int sy,
                              int srcW = TILE_SIZE, int srcH = TILE_SIZE)
{
    SDL_Surface* loaded = IMG_Load(path);
    if (!loaded)
        return nullptr;

    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!rgba)
        return nullptr;

    keyOutBlackPixels(rgba);

    SDL_Surface* tile = SDL_CreateSurface(srcW, srcH, SDL_PIXELFORMAT_RGBA32);
    if (!tile) {
        SDL_DestroySurface(rgba);
        return nullptr;
    }

    SDL_Rect src{ sx, sy, srcW, srcH };
    SDL_BlitSurface(rgba, &src, tile, nullptr);
    SDL_DestroySurface(rgba);

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, tile);
    SDL_DestroySurface(tile);
    if (tex)
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

static SDL_Texture* loadCoinTexture(SDL_Renderer* renderer, const char* path)
{
    SDL_Surface* loaded = IMG_Load(path);
    if (!loaded)
        return nullptr;

    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!rgba)
        return nullptr;

    stripWhiteBackground(rgba);

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, rgba);
    SDL_DestroySurface(rgba);
    if (tex)
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

static SDL_Texture* loadPlayerSheet(SDL_Renderer* renderer, const char* path)
{
    SDL_Surface* loaded = IMG_Load(path);
    if (!loaded)
        return nullptr;

    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!rgba)
        return nullptr;

    stripCheckerboardBackground(rgba);

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, rgba);
    SDL_DestroySurface(rgba);
    if (tex)
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

static SDL_Texture* makeSolidTex(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET, 2, 2);
    if (!tex)
        return nullptr;
    SDL_SetRenderTarget(renderer, tex);
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);
    return tex;
}

Game::Game()
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer("Fun Run — Demo", SCREEN_W, SCREEN_H, 0, &_window, &_renderer);

    _spritesheet = loadPlayerSheet(_renderer, "res/spritesheet.png");
    _terrainTile = loadTerrainTexture(_renderer, TERRAIN_TILE_PATH);
    _finishPole  = makeSolidTex(_renderer, 210, 50, 50);
    _finishSign  = cropTile(_renderer, "res/tiles.png", TILE_SIGN_X, TILE_SIGN_Y);
    _coinTex      = loadCoinTexture(_renderer, "res/coin.png");
    _questionTile = cropTile(_renderer, "res/tiles.png", 1032, 1584, 256, 256);
    _menuTexture  = IMG_LoadTexture(_renderer, "res/menu.png");

    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity    = { 0.f, 9.81f };
    _physicsWorld       = b2CreateWorld(&worldDef);

    BuiltLevel level = buildTileLevel(_physicsWorld, _tileMap,
                                     _finishPole, _finishSign, _coinTex);
    _coinSpawns            = level.coinSpawns;
    _qBlockBodies          = std::move(level.qBlockBodies);
    _levelWidthPx          = level.widthPx;
    _levelHeightPx         = level.heightPx;
    _mapCameraY            = level.cameraY;
    _playerStart           = level.playerStart;
    _finishSensor          = level.finishSensor;

    init_world();
    _running = true;
}

Game::~Game()
{
    b2DestroyWorld(_physicsWorld);
    SDL_DestroyTexture(_spritesheet);
    SDL_DestroyTexture(_terrainTile);
    SDL_DestroyTexture(_finishPole);
    SDL_DestroyTexture(_finishSign);
    SDL_DestroyTexture(_coinTex);
    SDL_DestroyTexture(_questionTile);
    SDL_DestroyTexture(_menuTexture);
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

void Game::init_world()
{
    createSensorArea(_physicsWorld, nullptr,
                     _finishSensor, 256.f, 140.f,
                     SensorType::FinishLine);
}

bool Game::any_player_finished() const
{
    static const bagel::Mask mask = bagel::MaskBuilder()
        .set<PlayerStateComponent>()
        .build();
    static int q = bagel::World::createQuery(mask);

    for (bagel::Entity e = bagel::World::first(q); !bagel::World::eof(q); e = bagel::World::next(q)) {
        if (e.get<PlayerStateComponent>().has_finished)
            return true;
    }
    return false;
}

void Game::reset_race()
{
    static const bagel::Mask mask = bagel::MaskBuilder()
        .set<TransformComponent>()
        .set<PhysicsBodyComponent>()
        .set<PlayerStateComponent>()
        .build();
    static int q = bagel::World::createQuery(mask);

    for (bagel::Entity e = bagel::World::first(q); !bagel::World::eof(q); e = bagel::World::next(q)) {
        auto& state = e.get<PlayerStateComponent>();
        if (!state.has_finished)
            continue;

        state.has_finished = false;
        state.score        = 0;
        auto& phys = e.get<PhysicsBodyComponent>();
        b2Body_SetTransform(phys.body_id,
                            { _playerStart.x / BOX_SCALE, _playerStart.y / BOX_SCALE },
                            b2Rot_identity);
        b2Body_SetLinearVelocity(phys.body_id, { 0.f, 0.f });
        e.get<TransformComponent>().position = _playerStart;
    }

    if (false) {
        // Obsolete: _camera logic removed
    }

    destroyAllCoins();
    spawnCoins(_physicsWorld, _coinTex, _coinSpawns);

    SDL_SetWindowTitle(_window, "Fun Run — Demo");
}

void Game::process_menu_input(const SDL_Event& e)
{
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.scancode == SDL_SCANCODE_LEFT) {
            _menuSelection--;
            if (_menuSelection < 0) _menuSelection = 3;
        } else if (e.key.scancode == SDL_SCANCODE_RIGHT) {
            _menuSelection++;
            if (_menuSelection > 3) _menuSelection = 0;
        } else if (e.key.scancode == SDL_SCANCODE_RETURN || e.key.scancode == SDL_SCANCODE_KP_ENTER) {
            if (_menuSelection == 0) {
                _numPlayers = 1;
            } else if (_menuSelection == 1) {
                _numPlayers = 2;
            }
            if (_menuSelection <= 1) {
                // Clear existing players/cameras if any
                static const bagel::Mask pMask = bagel::MaskBuilder().set<PlayerStateComponent>().build();
                static int pQ = bagel::World::createQuery(pMask);
                std::vector<bagel::Entity> toDestroy;
                for (bagel::Entity e = bagel::World::first(pQ); !bagel::World::eof(pQ); e = bagel::World::next(pQ)) {
                    b2DestroyBody(e.get<PhysicsBodyComponent>().body_id);
                    toDestroy.push_back(e);
                }
                static const bagel::Mask cMask = bagel::MaskBuilder().set<CameraComponent>().build();
                static int cQ = bagel::World::createQuery(cMask);
                for (bagel::Entity e = bagel::World::first(cQ); !bagel::World::eof(cQ); e = bagel::World::next(cQ)) {
                    toDestroy.push_back(e);
                }
                for (bagel::Entity e : toDestroy) e.destroy();

                if (_numPlayers == 1) {
                    SDL_Rect vp1 = { 0, 0, SCREEN_W, SCREEN_H };
                    createCamera({ 0.f, _mapCameraY }, 0, vp1);
                    createPlayer(_physicsWorld, _spritesheet, _playerStart, 0);
                } else {
                    SDL_Rect vp1 = { 0, 0, SCREEN_W, SCREEN_H / 2 - 2 };
                    createCamera({ 0.f, _mapCameraY }, 0, vp1);
                    SDL_Rect vp2 = { 0, SCREEN_H / 2 + 2, SCREEN_W, SCREEN_H / 2 - 2 };
                    createCamera({ 0.f, _mapCameraY }, 1, vp2);
                    createPlayer(_physicsWorld, _spritesheet, _playerStart, 0);
                    SDL_FPoint p2Start = _playerStart;
                    p2Start.x -= 40.f;
                    createPlayer(_physicsWorld, _spritesheet, p2Start, 1);
                }

                destroyAllCoins();
                spawnCoins(_physicsWorld, _coinTex, _coinSpawns);

                _state = GameState::Playing;
            }
        }
    }
}

void Game::render_menu()
{
    SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
    SDL_RenderClear(_renderer);

    if (_menuTexture) {
        SDL_FRect dst = { 0, 0, (float)SCREEN_W, (float)SCREEN_H };
        SDL_RenderTexture(_renderer, _menuTexture, nullptr, &dst);
    }

    // Draw selection highlight over the approximate button areas based on the image
    // Note: These coordinates are approximations for 1280x720 and might need tuning
    SDL_FRect highlight = { 0, 0, 0, 0 };
    if (_menuSelection == 0) { // Single Player
        highlight = { 100.f, 430.f, 250.f, 200.f };
    } else if (_menuSelection == 1) { // 2 Player Co-op
        highlight = { 360.f, 430.f, 250.f, 200.f };
    } else if (_menuSelection == 2) { // Highest Score
        highlight = { 760.f, 430.f, 250.f, 200.f };
    } else if (_menuSelection == 3) { // Settings/Credits
        highlight = { 1020.f, 430.f, 180.f, 200.f };
    }

    SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(_renderer, 255, 255, 0, 100); // Yellow glow
    SDL_RenderFillRect(_renderer, &highlight);
    
    SDL_SetRenderDrawColor(_renderer, 255, 255, 0, 255); // Solid yellow border
    SDL_RenderRect(_renderer, &highlight);

    SDL_RenderPresent(_renderer);
}

void Game::return_to_menu()
{
    _state = GameState::Menu;
}

void Game::run()
{
    SystemContext ctx{
        _physicsWorld,
        _renderer,
        _window,
        _spritesheet,
        1.f / TARGET_FPS,
        nullptr,
        _levelWidthPx,
        _levelHeightPx,
        _mapCameraY,
        CAMERA_ZOOM,
        &_tileMap,
        _terrainTile,
        _questionTile,
        &_qBlockBodies,
        &_bouncingQBlocks
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

            if (_state == GameState::Menu) {
                process_menu_input(ev);
            } else {
                if (ev.type == SDL_EVENT_KEY_DOWN &&
                    ev.key.scancode == SDL_SCANCODE_R &&
                    ev.key.repeat == 0 &&
                    any_player_finished())
                    reset_race();

                if (ev.type == SDL_EVENT_KEY_DOWN &&
                    ev.key.scancode == SDL_SCANCODE_M &&
                    ev.key.repeat == 0 &&
                    any_player_finished())
                    return_to_menu();

                if (eventCount < 64)
                    events[eventCount++] = ev;
            }
        }

        if (_state == GameState::Menu) {
            render_menu();
        } else {
            input_system(events, eventCount);
            controller_system(ctx);
            physics_system(ctx);
            sensor_system(ctx);
            damage_system(ctx);
            qblock_system(ctx);
            camera_system(ctx);
            render_system(ctx);
        }

        const Uint64 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < static_cast<Uint64>(GAME_FRAME_MS))
            SDL_Delay(static_cast<Uint32>(GAME_FRAME_MS - elapsed));
        frameStart += GAME_FRAME_MS;
    }
}
