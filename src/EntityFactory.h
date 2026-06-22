#pragma once

#include <cstdint>
#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include "bagel.h"
#include "TileConfig.h"

// Free-function blueprints for the 5 entity types defined in the
// Fun Run ECS Architecture & Implementation Plan. Each factory creates
// the entity, attaches its component set via addAll(...), and (where
// applicable) creates the matching Box2D body. Pixel positions are
// converted to Box2D meters internally via BOX_SCALE.m

bagel::Entity createPlayer(
    b2WorldId    world,
    SDL_Texture* tex,
    SDL_FPoint   startPosPx,
    int          playerIndex);

bagel::Entity createPlatform(
    b2WorldId    world,
    SDL_Texture* tex,
    SDL_FPoint   posPx,
    float        widthPx,
    float        heightPx);

// Invisible static collider — used with map.png artwork (no drawable).
bagel::Entity createPhysicsPlatform(
    b2WorldId  world,
    SDL_FPoint centerPx,
    float      widthPx,
    float      heightPx);

bagel::Entity createItemBox(
    b2WorldId    world,
    SDL_Texture* tex,
    SDL_FPoint   posPx,
    int          sensorTypeId);

bagel::Entity createProjectile(
    b2WorldId    world,
    SDL_Texture* tex,
    SDL_FPoint   posPx,
    SDL_FPoint   velocityPxPerSec,
    uint64_t     ownerEntityId);

bagel::Entity createDecoration(
    SDL_Texture* tex,
    SDL_FPoint   posPx,
    float        widthPx,
    float        heightPx);

bagel::Entity createSensorArea(
    b2WorldId    world,
    SDL_Texture* tex,
    SDL_FPoint   centerPx,
    float        widthPx,
    float        heightPx,
    int          sensorTypeId);

bagel::Entity createCoin(
    b2WorldId    world,
    SDL_Texture* tex,
    SDL_FPoint   centerPx,
    int          value = COIN_SCORE_VALUE);

bagel::Entity createCamera(SDL_FPoint startPosPx, int targetPlayerIndex, SDL_Rect viewport);
