#pragma once

#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include "bagel.h"

// Shared per-frame context passed to systems that need access to
// engine-level resources (rather than reaching for globals).
struct SystemContext {
    b2WorldId     physicsWorld;
    SDL_Renderer* renderer;
    SDL_Texture*  spritesheet;
    bagel::Entity camera;
    float         frameDtSec;
    SDL_Texture*  mapTexture;
    float         mapWidthPx;
    float         mapHeightPx;
    float         zoom;         // render scale (1.0 = no zoom, 1.5 = zoomed in)
};

// Systems run once per frame, in this order (see Game::run):
//   input -> controller -> physics -> sensor -> damage -> camera -> render
void input_system(const SDL_Event* events, int eventCount);
void controller_system(const SystemContext& ctx);
void physics_system(const SystemContext& ctx);
void sensor_system(const SystemContext& ctx);
void damage_system(const SystemContext& ctx);
void camera_system(const SystemContext& ctx);
void render_system(const SystemContext& ctx);
