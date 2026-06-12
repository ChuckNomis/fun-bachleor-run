#include "EntityFactory.h"
#include "Components.h"
#include "Game.h"
#include "SpriteConfig.h"

using bagel::Entity;

// On-screen rendered size of the player character
static constexpr float PLAYER_DRAW_W  = 96.f;
static constexpr float PLAYER_DRAW_H  = 96.f;

bagel::Entity createPlayer(b2WorldId world, SDL_Texture* tex,
                           SDL_FPoint startPosPx, int /*playerIndex*/)
{
    b2BodyDef bodyDef       = b2DefaultBodyDef();
    bodyDef.type            = b2_dynamicBody;
    bodyDef.position        = { startPosPx.x / BOX_SCALE, startPosPx.y / BOX_SCALE };
    bodyDef.motionLocks.angularZ = true;  // prevent tumbling
    b2BodyId body           = b2CreateBody(world, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density    = 1.0f;
    shapeDef.material.friction = 0.f; // slide off platform edges instead of snagging
    shapeDef.enableSensorEvents = true;
    shapeDef.filter.categoryBits = PHYS_CAT_DEFAULT;
    shapeDef.filter.maskBits     = PHYS_CAT_DEFAULT; // don't collide with Q-block bodies
    // Capsule slides past platform corners better than a box.
    b2Capsule capsule = {
        { 0.f, -21.f / BOX_SCALE },
        { 0.f,  21.f / BOX_SCALE },
        15.f / BOX_SCALE
    };
    b2ShapeId shape = b2CreateCapsuleShape(body, &shapeDef, &capsule);

    Entity e = Entity::create();
    e.addAll(
        TransformComponent  { startPosPx, 0.f },
        DrawableComponent   { tex,
                              { SPRITE_CROP_X, SPRITE_CROP_Y, SPRITE_CROP_W, SPRITE_CROP_H },
                              { 0, 0, PLAYER_DRAW_W, PLAYER_DRAW_H },
                              SDL_FLIP_NONE },
        PhysicsBodyComponent{ body, shape, false, 0, 0 },
        PlayerInputComponent{ false, false, false, false, false, 0 },
        PlayerStateComponent{ 3, 1.0f, -1, false, false, 0 },
        GravityShiftComponent{ false, 0.f, 3000.f },
        AnimationComponent  { 0, 0.f, 80.f }
    );
    return e;
}

bagel::Entity createPlatform(b2WorldId world, SDL_Texture* tex,
                             SDL_FPoint posPx, float widthPx, float heightPx)
{
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type      = b2_staticBody;
    bodyDef.position  = { posPx.x / BOX_SCALE, posPx.y / BOX_SCALE };
    b2BodyId body     = b2CreateBody(world, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    b2Polygon box       = b2MakeBox(widthPx / 2.f / BOX_SCALE, heightPx / 2.f / BOX_SCALE);
    b2ShapeId shape     = b2CreatePolygonShape(body, &shapeDef, &box);

    Entity e = Entity::create();
    e.addAll(
        TransformComponent  { posPx, 0.f },
        DrawableComponent   { tex, {0, 0, widthPx, heightPx}, {0, 0, widthPx, heightPx}, 0 },
        PhysicsBodyComponent{ body, shape, false, 0, 0 }
    );
    return e;
}

bagel::Entity createPhysicsPlatform(b2WorldId world, SDL_FPoint centerPx,
                                    float widthPx, float heightPx)
{
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type      = b2_staticBody;
    bodyDef.position  = { centerPx.x / BOX_SCALE, centerPx.y / BOX_SCALE };
    b2BodyId body     = b2CreateBody(world, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    b2Polygon box = b2MakeBox(widthPx / 2.f / BOX_SCALE, heightPx / 2.f / BOX_SCALE);
    b2ShapeId shape = b2CreatePolygonShape(body, &shapeDef, &box);

    Entity e = Entity::create();
    e.addAll(
        TransformComponent  { centerPx, 0.f },
        PhysicsBodyComponent{ body, shape, false, 0, 0 }
    );
    return e;
}

bagel::Entity createItemBox(b2WorldId world, SDL_Texture* tex,
                            SDL_FPoint posPx, int sensorTypeId)
{
    return createSensorArea(world, tex, posPx, 24.f, 24.f, sensorTypeId);
}

bagel::Entity createSensorArea(b2WorldId world, SDL_Texture* tex,
                               SDL_FPoint centerPx, float widthPx, float heightPx,
                               int sensorTypeId)
{
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type      = b2_staticBody;
    bodyDef.position  = { centerPx.x / BOX_SCALE, centerPx.y / BOX_SCALE };
    b2BodyId body     = b2CreateBody(world, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.isSensor           = true;
    shapeDef.enableSensorEvents = true;
    b2Polygon box = b2MakeBox(widthPx / 2.f / BOX_SCALE, heightPx / 2.f / BOX_SCALE);
    b2ShapeId shape = b2CreatePolygonShape(body, &shapeDef, &box);

    Entity e = Entity::create();
    if (tex) {
        e.addAll(
            TransformComponent  { centerPx, 0.f },
            DrawableComponent   { tex, {0, 0, widthPx, heightPx}, {0, 0, widthPx, heightPx}, 0 },
            SensorAreaComponent { shape, sensorTypeId, false }
        );
    } else {
        e.addAll(
            TransformComponent  { centerPx, 0.f },
            SensorAreaComponent { shape, sensorTypeId, false }
        );
    }
    return e;
}

bagel::Entity createProjectile(b2WorldId world, SDL_Texture* tex, SDL_FPoint posPx,
                               SDL_FPoint velocityPxPerSec, uint64_t ownerEntityId)
{
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type      = b2_dynamicBody;
    bodyDef.position  = { posPx.x / BOX_SCALE, posPx.y / BOX_SCALE };
    b2BodyId body     = b2CreateBody(world, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density    = 1.0f;
    b2Circle circle     = { {0, 0}, 8.f / BOX_SCALE };
    b2ShapeId shape     = b2CreateCircleShape(body, &shapeDef, &circle);

    b2Body_SetLinearVelocity(body, { velocityPxPerSec.x / BOX_SCALE, velocityPxPerSec.y / BOX_SCALE });

    Entity e = Entity::create();
    e.addAll(
        TransformComponent  { posPx, 0.f },
        DrawableComponent   { tex, {0, 0, 16, 16}, {0, 0, 16, 16}, 0 },
        PhysicsBodyComponent{ body, shape, false, 0, 0 },
        TrapComponent       { 0.5f, 1.5f, ownerEntityId }
    );
    return e;
}

bagel::Entity createDecoration(SDL_Texture* tex,
                               SDL_FPoint posPx, float widthPx, float heightPx)
{
    Entity e = Entity::create();
    e.addAll(
        TransformComponent { posPx, 0.f },
        DrawableComponent  { tex, {0, 0, widthPx, heightPx}, {0, 0, widthPx, heightPx}, 0 }
    );
    return e;
}

bagel::Entity createCoin(b2WorldId world, SDL_Texture* tex,
                         SDL_FPoint centerPx, int value)
{
    constexpr float kCoinDraw = 36.f;
    Entity e = createSensorArea(world, tex, centerPx, kCoinDraw, kCoinDraw,
                                SensorType::Coin);
    e.add(CoinComponent{ value });
    return e;
}

bagel::Entity createCamera(SDL_FPoint startPosPx)
{
    Entity e = Entity::create();
    e.add(TransformComponent{ startPosPx, 0.f });
    return e;
}
