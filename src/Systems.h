#pragma once

#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include <unordered_map>
#include "bagel.h"
#include "TileMap.h"

// Shared per-frame context passed to systems that need access to
// engine-level resources (rather than reaching for globals).
struct SystemContext {
    b2WorldId     physicsWorld;
    SDL_Renderer* renderer;
    SDL_Window*   window;
    SDL_Texture*  spritesheet;
    float         frameDtSec;
    SDL_Texture*  mapTexture;
    float         mapWidthPx;
    float         mapHeightPx;
    float         mapCameraY;
    float         zoom;
    TileMap*       tileMap      = nullptr;
    SDL_Texture*  terrainTile   = nullptr;
    SDL_Texture*  questionTile  = nullptr;
    std::unordered_map<uint32_t, bagel::Entity>* qBlockBodies   = nullptr;
    // key = col|(row<<16), value = elapsed seconds into bounce animation
    std::unordered_map<uint32_t, float>*         bouncingQBlocks = nullptr;
};

// Systems run once per frame, in this order (see Game::run):
//   input -> controller -> physics -> sensor -> damage -> qblock -> camera -> render
void input_system(const SDL_Event* events, int eventCount);
void controller_system(const SystemContext& ctx);
void physics_system(const SystemContext& ctx);
void sensor_system(const SystemContext& ctx);
void damage_system(const SystemContext& ctx);
void qblock_system(const SystemContext& ctx);
void camera_system(const SystemContext& ctx);
void render_system(const SystemContext& ctx);
void audio_system(const SystemContext& ctx);
