#include "Systems.h"
#include "Components.h"
#include "Game.h"

using bagel::Entity;
using bagel::Mask;
using bagel::MaskBuilder;
using bagel::World;

namespace {
    constexpr float RAD_TO_DEG = 180.f / 3.14159265358979323846f;
}

// ──────────────────────────────────────────────────────────────
// InputSystem — maps SDL key events onto each player's
// PlayerInputComponent. Edge-triggered fields are reset every
// frame, then set by the matching key-down events.
// ──────────────────────────────────────────────────────────────
void input_system(const SDL_Event* events, int eventCount)
{
    static const Mask mask = MaskBuilder().set<PlayerInputComponent>().build();
    static int q = World::createQuery(mask);

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        auto& in = e.get<PlayerInputComponent>();
        in.jump_pressed          = false;
        in.use_item_pressed      = false;
        in.gravity_shift_pressed = false;
        // TODO: map SDL_Scancode -> player index, set the booleans above
        //       and move_left/move_right based on key-down/key-up events.
    }

    for (int i = 0; i < eventCount; ++i) {
        (void)events[i];
        // TODO: dispatch each event to the right player's PlayerInputComponent
    }
}

// ──────────────────────────────────────────────────────────────
// PlayerControllerSystem — reads input/state, drives the Box2D
// body (velocity/forces), and toggles personal gravity shift.
// ──────────────────────────────────────────────────────────────
void controller_system(const SystemContext& /*ctx*/)
{
    static const Mask mask = MaskBuilder()
        .set<PlayerInputComponent>()
        .set<PlayerStateComponent>()
        .set<PhysicsBodyComponent>()
        .set<GravityShiftComponent>()
        .build();
    static int q = World::createQuery(mask);

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        const auto& in    = e.get<PlayerInputComponent>();
        const auto& state = e.get<PlayerStateComponent>();
        const auto& phys  = e.get<PhysicsBodyComponent>();
        auto& shift       = e.get<GravityShiftComponent>();

        (void)in; (void)state; (void)phys; (void)shift;
        // TODO: apply move_left/move_right velocities, jump impulses
        //       (scaled by current_speed_multiplier), and on
        //       gravity_shift_pressed toggle shift.is_inverted and call
        //       b2Body_SetGravityScale(phys.body_id, shift.is_inverted ? -1.f : 1.f)
    }
}

// ──────────────────────────────────────────────────────────────
// PhysicsSystem — steps Box2D and writes resulting transforms
// back into TransformComponent (meters -> pixels via BOX_SCALE).
// ──────────────────────────────────────────────────────────────
void physics_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder()
        .set<TransformComponent>()
        .set<PhysicsBodyComponent>()
        .build();
    static int q = World::createQuery(mask);

    b2World_Step(ctx.physicsWorld, ctx.frameDtSec, 4);

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        const auto& phys = e.get<PhysicsBodyComponent>();
        const b2Transform t = b2Body_GetTransform(phys.body_id);
        e.get<TransformComponent>() = {
            { t.p.x * BOX_SCALE, t.p.y * BOX_SCALE },
            b2Rot_GetAngle(t.q) * RAD_TO_DEG
        };
        // TODO: update is_grounded from contact events
    }
}

// ──────────────────────────────────────────────────────────────
// SensorCollisionSystem — reads Box2D sensor events, matches
// them against SensorAreaComponent entities (item boxes, finish
// lines), updates player inventory, and removes consumed boxes.
// ──────────────────────────────────────────────────────────────
void sensor_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder().set<SensorAreaComponent>().build();
    static int q = World::createQuery(mask);

    const b2SensorEvents events = b2World_GetSensorEvents(ctx.physicsWorld);
    for (int i = 0; i < events.beginCount; ++i) {
        (void)events.beginEvents[i];
        // TODO: resolve visitor/sensor shapes -> entities, walk the query
        //       above to find the matching SensorAreaComponent, mark
        //       is_triggered_this_frame, update PlayerStateComponent
        //       (current_powerup_id), and queue the item box for destroy().
    }

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        auto& sensor = e.get<SensorAreaComponent>();
        if (sensor.is_triggered_this_frame) {
            sensor.is_triggered_this_frame = false;
            // TODO: e.destroy() once the item has been applied to the player
            //       (do this after the query loop completes, never mid-iteration).
        }
    }
}

// ──────────────────────────────────────────────────────────────
// DamageSystem — processes trap/hazard overlaps with players,
// applies penalties/stun, and tracks elimination.
// ──────────────────────────────────────────────────────────────
void damage_system(const SystemContext& /*ctx*/)
{
    static const Mask mask = MaskBuilder()
        .set<TrapComponent>()
        .set<PhysicsBodyComponent>()
        .build();
    static int q = World::createQuery(mask);

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        const auto& trap = e.get<TrapComponent>();
        (void)trap;
        // TODO: check Box2D contact events for player overlap; on hit,
        //       apply speed_penalty_factor / stun_duration_seconds to the
        //       player's PlayerStateComponent and decrement lives,
        //       setting is_eliminated when lives reaches zero.
    }
}

// ──────────────────────────────────────────────────────────────
// CameraSystem — tracks the lead (furthest-forward, non-eliminated)
// racer and anchors the camera entity's TransformComponent to it.
// ──────────────────────────────────────────────────────────────
void camera_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder()
        .set<TransformComponent>()
        .set<PlayerStateComponent>()
        .build();
    static int q = World::createQuery(mask);

    float maxX = 0.f;
    bool found = false;
    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        const auto& state = e.get<PlayerStateComponent>();
        if (state.is_eliminated) continue;

        const float px = e.get<TransformComponent>().position.x;
        if (!found || px > maxX) {
            maxX  = px;
            found = true;
        }
    }

    if (found && ctx.camera.has<TransformComponent>()) {
        // TODO: replace the snap-to with a lerp toward maxX for smooth tracking
        ctx.camera.get<TransformComponent>().position.x = maxX;
    }
}

// ──────────────────────────────────────────────────────────────
// RenderSystem — clears the renderer, offsets sprites by the
// camera position, and draws every Transform+Drawable entity.
// ──────────────────────────────────────────────────────────────
void render_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder()
        .set<TransformComponent>()
        .set<DrawableComponent>()
        .build();
    static int q = World::createQuery(mask);

    SDL_RenderClear(ctx.renderer);

    SDL_FPoint camPos{0, 0};
    if (ctx.camera.has<TransformComponent>())
        camPos = ctx.camera.get<TransformComponent>().position;

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        const auto& tr = e.get<TransformComponent>();
        const auto& dr = e.get<DrawableComponent>();

        SDL_FRect dest = dr.dest_dimensions;
        dest.x = tr.position.x - camPos.x;
        dest.y = tr.position.y - camPos.y;

        SDL_RenderTextureRotated(
            ctx.renderer, dr.texture, &dr.src_rect, &dest,
            tr.rotation_degrees, nullptr,
            static_cast<SDL_FlipMode>(dr.flip_flags));
    }

    SDL_RenderPresent(ctx.renderer);
}
