#pragma once

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <box2d/box2d.h>
#include "bagel.h"
#include "TileMap.h"
#include <vector>

inline constexpr float BOX_SCALE     = 50.0f;   // pixels per Box2D meter
inline constexpr int   SCREEN_W      = 1280;
inline constexpr int   SCREEN_H      = 720;
inline constexpr int   TARGET_FPS    = 60;
inline constexpr int   GAME_FRAME_MS = 1000 / TARGET_FPS;
inline constexpr float CAMERA_ZOOM   = 1.5f;    // render scale (>1 = zoomed in)

class Game {
public:
    Game();
    ~Game();

    void run();

private:
    void init_world();
    void reset_race();
    bool any_player_finished() const;

    SDL_Window*   _window      = nullptr;
    SDL_Renderer* _renderer    = nullptr;
    SDL_Texture*  _spritesheet  = nullptr;
    SDL_Texture*  _terrainTile  = nullptr;
    SDL_Texture*  _finishPole   = nullptr;
    SDL_Texture*  _finishSign   = nullptr;
    SDL_Texture*  _coinTex      = nullptr;
    std::vector<SDL_FPoint> _coinSpawns;
    TileMap       _tileMap;
    float         _levelWidthPx  = 3200.f;
    float         _levelHeightPx = 640.f;
    float         _mapCameraY   = 0.f;
    SDL_FPoint    _playerStart  { 200.f, 330.f };
    SDL_FPoint    _finishSensor { 3050.f, 450.f };
    b2WorldId     _physicsWorld{};
    bool          _running     = false;

    bagel::Entity _camera{{-1}};
};
