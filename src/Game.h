#pragma once

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <box2d/box2d.h>
#include "bagel.h"

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

    SDL_Window*   _window      = nullptr;
    SDL_Renderer* _renderer    = nullptr;
    SDL_Texture*  _spritesheet  = nullptr;
    SDL_Texture*  _mapTexture   = nullptr;  // unused in simple level, kept for SystemContext
    SDL_Texture*  _platTexture   = nullptr;  // solid green for platforms
    SDL_Texture*  _groundTexture = nullptr;  // solid brown for ground
    SDL_Texture*  _startTex      = nullptr;  // bright green start-line pole
    SDL_Texture*  _finishTex     = nullptr;  // red finish-line pole
    b2WorldId     _physicsWorld{};
    bool          _running     = false;

    bagel::Entity _camera{{-1}};
};
