#include "EntityFactory.h"
#include "Components.h"
#include "Game.h"

using bagel::Entity;

bagel::Entity createPlayer(b2WorldId world, SDL_Texture* tex,
                           SDL_FPoint startPosPx, int /*playerIndex*/)
{
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type      = b2_dynamicBody;
    bodyDef.position  = { startPosPx.x / BOX_SCALE, startPosPx.y / BOX_SCALE };
    b2BodyId body     = b2CreateBody(world, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density    = 1.0f;
    shapeDef.friction   = 0.3f;
    b2Polygon box       = b2MakeBox(16.f / BOX_SCALE, 24.f / BOX_SCALE);
    b2ShapeId shape     = b2CreatePolygonShape(body, &shapeDef, &box);

    Entity e = Entity::create();
    e.addAll(
        TransformComponent  { startPosPx, 0.f },
        DrawableComponent   { tex, {0, 0, 32, 48}, {0, 0, 32, 48}, 0 },
        PhysicsBodyComponent{ body, shape, false },
        PlayerInputComponent{ false, false, false, false, false },
        PlayerStateComponent{ 3, 1.0f, -1, false },
        GravityShiftComponent{ false, 0.f, 3000.f }
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
        PhysicsBodyComponent{ body, shape, false }
    );
    return e;
}

bagel::Entity createItemBox(SDL_Texture* tex, SDL_FPoint posPx, int sensorTypeId)
{
    Entity e = Entity::create();
    e.addAll(
        TransformComponent { posPx, 0.f },
        DrawableComponent  { tex, {0, 0, 24, 24}, {0, 0, 24, 24}, 0 },
        SensorAreaComponent{ sensorTypeId, false }
    );
    return e;
    // TODO: attach a Box2D sensor shape (isSensor = true, enableSensorEvents = true)
    //       once a static body for sensor-only entities is wired up.
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
        PhysicsBodyComponent{ body, shape, false },
        TrapComponent       { 0.5f, 1.5f, ownerEntityId }
    );
    return e;
}

bagel::Entity createCamera(SDL_FPoint startPosPx)
{
    Entity e = Entity::create();
    e.add(TransformComponent{ startPosPx, 0.f });
    return e;
}
