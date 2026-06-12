#pragma once

#include "TileConfig.h"
#include "TileMap.h"
#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include <bagel.h>
#include <unordered_map>
#include <vector>

struct BuiltLevel {
    float                                  widthPx     = 3200.f;
    float                                  heightPx    = 640.f;
    float                                  cameraY     = 0.f;
    SDL_FPoint                             playerStart { 200.f, 330.f };
    SDL_FPoint                             finishSensor{ 3050.f, 450.f };
    std::vector<SDL_FPoint>                coinSpawns;
    // key = col | (row << 16), value = the ECS entity owning the Q-block body
    std::unordered_map<uint32_t, bagel::Entity> qBlockBodies;
};

BuiltLevel buildTileLevel(b2WorldId world, TileMap& tileMap,
                          SDL_Texture* finishPoleTex, SDL_Texture* finishSignTex,
                          SDL_Texture* coinTex);

void spawnCoins(b2WorldId world, SDL_Texture* coinTex,
                const std::vector<SDL_FPoint>& spawns, int value = COIN_SCORE_VALUE);

void destroyAllCoins();
