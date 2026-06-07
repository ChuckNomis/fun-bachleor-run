#include "Systems.h"
#include "Components.h"
#include "Game.h"

using bagel::Entity;
using bagel::Mask;
using bagel::MaskBuilder;
using bagel::World;

namespace {
    constexpr float RAD_TO_DEG    = 180.f / 3.14159265358979323846f;
    constexpr float RUN_SPEED_MPS = 2.5f;
    constexpr float JUMP_VY       = 8.5f;
    constexpr int   JUMP_LOCK_FRAMES = 18; // ~0.3 s cooldown prevents apex re-jump

    // Spritesheet: 1774×887 px, 4 cols × 2 rows
    constexpr float SPRITE_FRAME_W = 1774.f / 4.f;
    constexpr float SPRITE_FRAME_H =  887.f / 2.f;
    constexpr int   SPRITE_COLS    = 4;
    constexpr int   TOTAL_FRAMES   = 8;

    // Physics body half-height (px) — used to bottom-align the sprite to the physics box.
    constexpr float PLAYER_BODY_HH = 24.f;
}

// ──────────────────────────────────────────────────────────────
void input_system(const SDL_Event* events, int eventCount)
{
    static const Mask mask = MaskBuilder().set<PlayerInputComponent>().build();
    static int q = World::createQuery(mask);

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        auto& in = e.get<PlayerInputComponent>();
        in.move_right            = true;
        in.move_left             = false;
        in.jump_pressed          = false;
        in.use_item_pressed      = false;
        in.gravity_shift_pressed = false;
    }

    for (int i = 0; i < eventCount; ++i) {
        const SDL_Event& ev = events[i];
        // key.repeat != 0 = OS key-repeat; ignore to avoid held-key jumps
        if (ev.type == SDL_EVENT_KEY_DOWN &&
            ev.key.scancode == SDL_SCANCODE_SPACE &&
            ev.key.repeat == 0)
        {
            for (Entity e = World::first(q); !World::eof(q); e = World::next(q))
                e.get<PlayerInputComponent>().jump_pressed = true;
        }
    }
}

// ──────────────────────────────────────────────────────────────
void controller_system(const SystemContext& /*ctx*/)
{
    static const Mask mask = MaskBuilder()
        .set<PlayerInputComponent>()
        .set<PlayerStateComponent>()
        .set<PhysicsBodyComponent>()
        .build();
    static int q = World::createQuery(mask);

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        const auto& in = e.get<PlayerInputComponent>();
        auto& phys     = e.get<PhysicsBodyComponent>();

        b2Vec2 vel = b2Body_GetLinearVelocity(phys.body_id);
        vel.x = in.move_right ? RUN_SPEED_MPS : 0.f;

        // Jump only when truly grounded and not within the lock window.
        // The lock prevents re-jumping at the apex (where vel.y ≈ 0 can
        // briefly satisfy the grounded heuristic in physics_system).
        if (in.jump_pressed && phys.is_grounded && phys.jump_lock_frames == 0) {
            vel.y = -JUMP_VY;
            phys.is_grounded     = false;
            phys.jump_lock_frames = JUMP_LOCK_FRAMES;
        }

        b2Body_SetLinearVelocity(phys.body_id, vel);
    }
}

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
        auto& phys = e.get<PhysicsBodyComponent>();
        const b2Transform t = b2Body_GetTransform(phys.body_id);
        e.get<TransformComponent>() = {
            { t.p.x * BOX_SCALE, t.p.y * BOX_SCALE },
            b2Rot_GetAngle(t.q) * RAD_TO_DEG
        };

        const b2Vec2 vel = b2Body_GetLinearVelocity(phys.body_id);
        // Grounded: very small downward velocity (settled on a surface).
        // Upper bound kept tight so apex-of-jump (vel.y ≈ 0) is NOT grounded.
        // jump_lock_frames acts as a second guard when this heuristic still fires.
        phys.is_grounded = (vel.y >= -0.3f && vel.y <= 0.5f);

        if (phys.jump_lock_frames > 0)
            --phys.jump_lock_frames;
    }
}

// ──────────────────────────────────────────────────────────────
void sensor_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder().set<SensorAreaComponent>().build();
    static int q = World::createQuery(mask);

    const b2SensorEvents events = b2World_GetSensorEvents(ctx.physicsWorld);
    (void)events;

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        auto& sensor = e.get<SensorAreaComponent>();
        if (sensor.is_triggered_this_frame)
            sensor.is_triggered_this_frame = false;
    }
}

// ──────────────────────────────────────────────────────────────
void damage_system(const SystemContext& /*ctx*/)
{
    static const Mask mask = MaskBuilder()
        .set<TrapComponent>()
        .set<PhysicsBodyComponent>()
        .build();
    static int q = World::createQuery(mask);
    (void)q;
}

// ──────────────────────────────────────────────────────────────
void camera_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder()
        .set<TransformComponent>()
        .set<PlayerStateComponent>()
        .build();
    static int q = World::createQuery(mask);

    float leadX = 0.f;
    bool  found = false;

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        if (e.get<PlayerStateComponent>().is_eliminated) continue;
        const float px = e.get<TransformComponent>().position.x;
        if (!found || px > leadX) { leadX = px; found = true; }
    }

    if (found && ctx.camera.has<TransformComponent>()) {
        // Visible world width shrinks with zoom: SCREEN_W / zoom
        const float visibleW = static_cast<float>(SCREEN_W) / ctx.zoom;
        float camX = leadX - visibleW / 2.f;
        if (camX < 0.f)                      camX = 0.f;
        if (camX > ctx.mapWidthPx - visibleW) camX = ctx.mapWidthPx - visibleW;
        ctx.camera.get<TransformComponent>().position.x = camX;
    }
}

// ──────────────────────────────────────────────────────────────
void render_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder()
        .set<TransformComponent>()
        .set<DrawableComponent>()
        .build();
    static int q = World::createQuery(mask);

    SDL_SetRenderDrawColor(ctx.renderer, 100, 180, 240, 255);
    SDL_RenderClear(ctx.renderer);

    SDL_FPoint camPos{0.f, 0.f};
    if (ctx.camera.has<TransformComponent>())
        camPos = ctx.camera.get<TransformComponent>().position;

    const float z = ctx.zoom;

    if (ctx.mapTexture) {
        SDL_FRect mapDest = { -camPos.x * z, 0.f,
                              ctx.mapWidthPx * z, ctx.mapHeightPx * z };
        SDL_RenderTexture(ctx.renderer, ctx.mapTexture, nullptr, &mapDest);
    }

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        const auto& tr = e.get<TransformComponent>();
        auto&       dr = e.get<DrawableComponent>();

        // Compute source rect (animated entities use the per-frame rect)
        SDL_FRect src = dr.src_rect;
        if (e.has<AnimationComponent>()) {
            auto& anim = e.get<AnimationComponent>();
            anim.timer_ms += ctx.frameDtSec * 1000.f;
            if (anim.timer_ms >= anim.frame_duration_ms) {
                anim.timer_ms -= anim.frame_duration_ms;
                anim.frame_index = (anim.frame_index + 1) % TOTAL_FRAMES;
            }
            const int col = anim.frame_index % SPRITE_COLS;
            const int row = anim.frame_index / SPRITE_COLS;
            src = { col * SPRITE_FRAME_W, row * SPRITE_FRAME_H,
                    SPRITE_FRAME_W, SPRITE_FRAME_H };
        }

        // World-space top-left of the draw rect
        const float baseW = dr.dest_dimensions.w;
        const float baseH = dr.dest_dimensions.h;

        const float worldLeft = tr.position.x - baseW / 2.f;
        const float worldTop  = e.has<AnimationComponent>()
            ? (tr.position.y + PLAYER_BODY_HH - baseH) // bottom-align to physics body
            : (tr.position.y - baseH / 2.f);            // center-align

        SDL_FRect dest {
            (worldLeft - camPos.x) * z,
            (worldTop  - camPos.y) * z,
            baseW * z,
            baseH * z
        };

        if (dr.texture) {
            SDL_FRect* srcPtr = e.has<AnimationComponent>() ? &src : nullptr;
            SDL_RenderTextureRotated(
                ctx.renderer, dr.texture, srcPtr, &dest,
                tr.rotation_degrees, nullptr,
                static_cast<SDL_FlipMode>(dr.flip_flags));
        }
    }

    SDL_RenderPresent(ctx.renderer);
}
