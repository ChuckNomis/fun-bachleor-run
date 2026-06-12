#include "Systems.h"
#include "Components.h"
#include "Game.h"
#include "SpriteConfig.h"
#include "TileConfig.h"
#include "TileMap.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

using bagel::Entity;
using bagel::Mask;
using bagel::MaskBuilder;
using bagel::World;

namespace {
    constexpr float RAD_TO_DEG    = 180.f / 3.14159265358979323846f;
    constexpr float RUN_SPEED_MPS    = 2.5f;
    constexpr float JUMP_VY          = 8.5f;
    constexpr int   JUMP_LOCK_FRAMES   = 18;
    constexpr int   COYOTE_FRAMES      = 8;
    constexpr int   JUMP_BUFFER_FRAMES = 10;
    constexpr float GROUND_PROBE_PX    = 14.f; // overlap box below the feet
    constexpr float ACCEL_PER_SEC    = 0.3f;   // speed multiplier gained per second
    constexpr float MAX_SPEED_MULT   = 3.0f;   // cap: 3× base speed

    // Physics body half-height (px) — capsule centers ±21px + radius 15px.
    constexpr float PLAYER_BODY_HH = 36.f;

    struct GroundProbeContext {
        b2ShapeId ownShape;
        bool      grounded;
    };

    bool groundOverlapCallback(b2ShapeId shapeId, void* context)
    {
        auto* ctx = static_cast<GroundProbeContext*>(context);
        if (B2_ID_EQUALS(shapeId, ctx->ownShape))
            return true;
        if (b2Shape_IsSensor(shapeId))
            return true;
        ctx->grounded = true;
        return false;
    }

    bool isPlayerGrounded(b2WorldId world, b2BodyId body, b2ShapeId ownShape)
    {
        const b2Transform t = b2Body_GetTransform(body);
        const float hh    = PLAYER_BODY_HH / BOX_SCALE;
        const float halfW = 8.f / BOX_SCALE;
        const float probe = GROUND_PROBE_PX / BOX_SCALE;
        const float feetY = t.p.y + hh;

        // Overlap a thin region under the feet. Unlike ray casts, this works
        // when the player is resting on a surface (initial overlap).
        b2AABB aabb;
        aabb.lowerBound = { t.p.x - halfW, feetY - 2.f / BOX_SCALE };
        aabb.upperBound = { t.p.x + halfW, feetY + probe };

        GroundProbeContext ctx{ ownShape, false };
        b2World_OverlapAABB(world, aabb, b2DefaultQueryFilter(), groundOverlapCallback, &ctx);
        if (ctx.grounded)
            return true;

        // Fallback: any solid contact on the body (works when wedged on terrain).
        const int capacity = b2Body_GetContactCapacity(body);
        if (capacity <= 0)
            return false;

        std::vector<b2ContactData> contacts(static_cast<std::size_t>(capacity));
        const int count = b2Body_GetContactData(body, contacts.data(), capacity);
        for (int i = 0; i < count; ++i) {
            const b2ContactData& contact = contacts[static_cast<std::size_t>(i)];
            if (contact.manifold.pointCount <= 0)
                continue;

            b2Vec2 n = contact.manifold.normal;
            if (B2_ID_EQUALS(contact.shapeIdB, ownShape)) {
                n.x = -n.x;
                n.y = -n.y;
            } else if (!B2_ID_EQUALS(contact.shapeIdA, ownShape)) {
                continue;
            }

            if (n.y > 0.3f)
                return true;
        }

        return false;
    }

    bool tryFindPlayerByShape(b2ShapeId visitorShapeId, Entity& out)
    {
        static const Mask mask = MaskBuilder()
            .set<PhysicsBodyComponent>()
            .set<PlayerStateComponent>()
            .build();
        static int q = World::createQuery(mask);

        for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
            if (B2_ID_EQUALS(e.get<PhysicsBodyComponent>().shape_id, visitorShapeId)) {
                out = e;
                return true;
            }
        }
        return false;
    }

    bool tryFindSensorByShape(b2ShapeId sensorShapeId, Entity& out)
    {
        static const Mask mask = MaskBuilder().set<SensorAreaComponent>().build();
        static int q = World::createQuery(mask);

        for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
            if (B2_ID_EQUALS(e.get<SensorAreaComponent>().shape_id, sensorShapeId)) {
                out = e;
                return true;
            }
        }
        return false;
    }
}

// ──────────────────────────────────────────────────────────────
namespace {
    bool isJumpKeyDown(const SDL_Event& ev)
    {
        if (ev.type != SDL_EVENT_KEY_DOWN || ev.key.repeat != 0)
            return false;
        switch (ev.key.scancode) {
        case SDL_SCANCODE_SPACE:
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_UP:
            return true;
        default:
            return false;
        }
    }
}

void input_system(const SDL_Event* events, int eventCount)
{
    static const Mask mask = MaskBuilder().set<PlayerInputComponent>().build();
    static int q = World::createQuery(mask);

    const bool* keys = SDL_GetKeyboardState(nullptr);
    const bool  spaceHeld = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    static bool wasJumpHeld = false;
    const bool  jumpEdge    = spaceHeld && !wasJumpHeld;
    wasJumpHeld = spaceHeld;

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        auto& in = e.get<PlayerInputComponent>();
        in.move_right            = true;
        in.move_left             = false;
        in.jump_pressed          = false;
        in.use_item_pressed      = false;
        in.gravity_shift_pressed = false;
        if (in.jump_buffer_frames > 0)
            --in.jump_buffer_frames;
    }

    auto queueJump = [&]() {
        for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
            auto& in = e.get<PlayerInputComponent>();
            in.jump_pressed = true;
            in.jump_buffer_frames = JUMP_BUFFER_FRAMES;
        }
    };

    if (jumpEdge)
        queueJump();

    for (int i = 0; i < eventCount; ++i) {
        if (isJumpKeyDown(events[i]))
            queueJump();
    }
}

// ──────────────────────────────────────────────────────────────
void controller_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder()
        .set<PlayerInputComponent>()
        .set<PlayerStateComponent>()
        .set<PhysicsBodyComponent>()
        .build();
    static int q = World::createQuery(mask);

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        auto& in    = e.get<PlayerInputComponent>();
        auto& phys  = e.get<PhysicsBodyComponent>();
        auto& state = e.get<PlayerStateComponent>();

        if (state.has_finished) {
            b2Body_SetLinearVelocity(phys.body_id, { 0.f, 0.f });
            continue;
        }

        // Detect wall hit: contact with a strong rightward normal = wall to the right
        bool hitWall = false;
        const int capContacts = b2Body_GetContactCapacity(phys.body_id);
        if (capContacts > 0) {
            std::vector<b2ContactData> contacts(static_cast<std::size_t>(capContacts));
            const int cnt = b2Body_GetContactData(phys.body_id, contacts.data(), capContacts);
            for (int i = 0; i < cnt; ++i) {
                if (contacts[static_cast<std::size_t>(i)].manifold.pointCount <= 0)
                    continue;
                b2Vec2 n = contacts[static_cast<std::size_t>(i)].manifold.normal;
                if (B2_ID_EQUALS(contacts[static_cast<std::size_t>(i)].shapeIdB, phys.shape_id)) {
                    n.x = -n.x; n.y = -n.y;
                } else if (!B2_ID_EQUALS(contacts[static_cast<std::size_t>(i)].shapeIdA, phys.shape_id)) {
                    continue;
                }
                if (n.x > 0.5f) { hitWall = true; break; }
            }
        }

        if (hitWall) {
            state.current_speed_multiplier = 1.0f;
        } else {
            state.current_speed_multiplier = std::min(
                state.current_speed_multiplier + ACCEL_PER_SEC * ctx.frameDtSec,
                MAX_SPEED_MULT);
        }

        b2Vec2 vel = b2Body_GetLinearVelocity(phys.body_id);
        vel.x = in.move_right ? RUN_SPEED_MPS * state.current_speed_multiplier : 0.f;

        const bool wantsJump = in.jump_pressed || in.jump_buffer_frames > 0;
        const bool canJump   = (phys.is_grounded || phys.coyote_frames > 0)
                            && wantsJump
                            && phys.jump_lock_frames == 0;

        if (canJump) {
            vel.y = -JUMP_VY;
            phys.is_grounded      = false;
            phys.coyote_frames    = 0;
            phys.jump_lock_frames = JUMP_LOCK_FRAMES;
            in.jump_buffer_frames = 0;
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

        phys.is_grounded = isPlayerGrounded(ctx.physicsWorld, phys.body_id, phys.shape_id);

        if (phys.is_grounded)
            phys.coyote_frames = COYOTE_FRAMES;
        else if (phys.coyote_frames > 0)
            --phys.coyote_frames;

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

    for (int i = 0; i < events.beginCount; ++i) {
        const b2SensorBeginTouchEvent& event = events.beginEvents[i];
        if (!b2Shape_IsValid(event.sensorShapeId))
            continue;

        Entity sensorEntity{{-1}};
        if (!tryFindSensorByShape(event.sensorShapeId, sensorEntity))
            continue;

        auto& sensor = sensorEntity.get<SensorAreaComponent>();
        sensor.is_triggered_this_frame = true;

        Entity player{{-1}};
        if (!tryFindPlayerByShape(event.visitorShapeId, player))
            continue;

        if (sensor.sensor_type_id == SensorType::FinishLine) {
            auto& state = player.get<PlayerStateComponent>();
            if (state.has_finished)
                continue;

            state.has_finished = true;
            auto& phys = player.get<PhysicsBodyComponent>();
            b2Body_SetLinearVelocity(phys.body_id, { 0.f, 0.f });

            if (ctx.window)
                SDL_SetWindowTitle(ctx.window, "Fun Run — You Win!");
            continue;
        }

        if (sensor.sensor_type_id == SensorType::Coin && sensorEntity.has<CoinComponent>()) {
            player.get<PlayerStateComponent>().score +=
                sensorEntity.get<CoinComponent>().value;

            const b2BodyId coinBody = b2Shape_GetBody(sensor.shape_id);
            b2DestroyBody(coinBody);
            sensorEntity.destroy();
        }
    }

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
void qblock_system(const SystemContext& ctx)
{
    if (!ctx.tileMap || !ctx.qBlockBodies || !ctx.bouncingQBlocks)
        return;

    static const Mask mask = MaskBuilder()
        .set<TransformComponent>()
        .set<PhysicsBodyComponent>()
        .set<PlayerStateComponent>()
        .build();
    static int q = World::createQuery(mask);

    TileMap&    map = *ctx.tileMap;
    const float ts  = static_cast<float>(TILE_SIZE);

    constexpr float BOUNCE_DURATION = 0.22f; // seconds

    // ── Advance active bounces; clear finished ones ──
    std::vector<uint32_t> done;
    for (auto& [key, elapsed] : *ctx.bouncingQBlocks) {
        elapsed += ctx.frameDtSec;
        if (elapsed >= BOUNCE_DURATION) {
            const int col = static_cast<int>(key & 0xFFFF);
            const int row = static_cast<int>(key >> 16);
            map.set(col, row, TileCell::Empty);
            done.push_back(key);
        }
    }
    for (uint32_t k : done)
        ctx.bouncingQBlocks->erase(k);

    // ── Detect new hits ──
    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        const auto& phys = e.get<PhysicsBodyComponent>();
        const b2Vec2 vel = b2Body_GetLinearVelocity(phys.body_id);
        if (vel.y >= 0.f)
            continue;

        const auto& pos    = e.get<TransformComponent>().position;
        const float probeY = pos.y - PLAYER_BODY_HH - 6.f;
        const int   tileRow = static_cast<int>(std::floor(probeY / ts));
        const int   colL    = static_cast<int>(std::floor((pos.x - 14.f) / ts));
        const int   colR    = static_cast<int>(std::floor((pos.x + 14.f) / ts));

        for (int col = colL; col <= colR; ++col) {
            if (map.at(col, tileRow) != TileCell::QuestionBlock)
                continue;

            const uint32_t key = static_cast<uint32_t>(col)
                               | (static_cast<uint32_t>(tileRow) << 16);
            if (ctx.bouncingQBlocks->count(key))
                continue; // already bouncing

            // Destroy physics body so player passes through after hit
            auto it = ctx.qBlockBodies->find(key);
            if (it != ctx.qBlockBodies->end()) {
                Entity qe = it->second;
                ctx.qBlockBodies->erase(it);
                b2DestroyBody(qe.get<PhysicsBodyComponent>().body_id);
                qe.destroy();
            }

            // Start bounce — tile stays visible during animation
            ctx.bouncingQBlocks->emplace(key, 0.f);
        }
    }
}

// ──────────────────────────────────────────────────────────────
void camera_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder()
        .set<TransformComponent>()
        .set<PlayerStateComponent>()
        .build();
    static int q = World::createQuery(mask);

    float leadX = 0.f, leadY = 0.f;
    bool  found = false;

    for (Entity e = World::first(q); !World::eof(q); e = World::next(q)) {
        if (e.get<PlayerStateComponent>().is_eliminated) continue;
        const auto& pos = e.get<TransformComponent>().position;
        if (!found || pos.x > leadX) { leadX = pos.x; leadY = pos.y; found = true; }
    }

    if (found && ctx.camera.has<TransformComponent>()) {
        const float visibleW = static_cast<float>(SCREEN_W) / ctx.zoom;
        const float visibleH = static_cast<float>(SCREEN_H) / ctx.zoom;
        float camX = leadX - visibleW * 0.5f;
        float camY = leadY - visibleH * 0.6f; // player in upper-third of view
        if (camX < 0.f)                       camX = 0.f;
        if (camX > ctx.mapWidthPx  - visibleW) camX = ctx.mapWidthPx  - visibleW;
        if (camY < 0.f)                       camY = 0.f;
        if (camY > ctx.mapHeightPx - visibleH) camY = ctx.mapHeightPx - visibleH;
        auto& cam = ctx.camera.get<TransformComponent>();
        cam.position.x = camX;
        cam.position.y = camY;
    }
}

namespace {

void renderTileMap(const SystemContext& ctx, SDL_FPoint camPos, float z)
{
    if (!ctx.tileMap || !ctx.terrainTile)
        return;

    const TileMap& map = *ctx.tileMap;
    const float    ts  = static_cast<float>(TILE_SIZE);
    const float    viewW = static_cast<float>(SCREEN_W) / z;
    const float    viewH = static_cast<float>(SCREEN_H) / z;

    const SDL_FRect fullSrc{
        0.f,
        0.f,
        static_cast<float>(TERRAIN_TEX_W),
        static_cast<float>(TERRAIN_TEX_H)
    };
    const SDL_FRect dirtSrc{
        0.f,
        static_cast<float>(TERRAIN_DIRT_Y),
        static_cast<float>(TERRAIN_TEX_W),
        static_cast<float>(TERRAIN_TEX_H - TERRAIN_DIRT_Y)
    };

    const int colBegin = std::max(0, static_cast<int>(std::floor(camPos.x / ts)) - 1);
    const int colEnd   = std::min(map.cols,
                                  static_cast<int>(std::ceil((camPos.x + viewW) / ts)) + 1);
    const int rowBegin = std::max(0, static_cast<int>(std::floor(camPos.y / ts)) - 1);
    const int rowEnd   = std::min(map.rows,
                                  static_cast<int>(std::ceil((camPos.y + viewH) / ts)) + 1);

    for (int row = rowBegin; row < rowEnd; ++row) {
        for (int col = colBegin; col < colEnd; ++col) {
            const TileCell cell = map.at(col, row);
            if (cell == TileCell::Empty)
                continue;

            const float worldLeft = static_cast<float>(col) * ts;
            const float worldTop  = static_cast<float>(row) * ts;
            SDL_FRect dest{
                (worldLeft - camPos.x) * z,
                (worldTop  - camPos.y) * z,
                ts * z,
                ts * z
            };

            const SDL_FRect* src = &fullSrc;
            if (cell == TileCell::Cloud) {
                // Draw a fluffy cloud shape using overlapping white rects
                SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ctx.renderer, 240, 245, 255, 200);
                SDL_FRect base  = { dest.x,              dest.y + dest.h * 0.42f, dest.w,        dest.h * 0.58f };
                SDL_FRect bump1 = { dest.x + dest.w*0.1f, dest.y + dest.h * 0.1f, dest.w * 0.55f, dest.h * 0.65f };
                SDL_FRect bump2 = { dest.x + dest.w*0.5f, dest.y + dest.h * 0.2f, dest.w * 0.4f,  dest.h * 0.5f  };
                SDL_RenderFillRect(ctx.renderer, &base);
                SDL_RenderFillRect(ctx.renderer, &bump1);
                SDL_RenderFillRect(ctx.renderer, &bump2);
                SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_NONE);
                continue;
            }

            if (cell == TileCell::QuestionBlock) {
                // Apply bounce offset if this tile is mid-animation
                if (ctx.bouncingQBlocks) {
                    const uint32_t key = static_cast<uint32_t>(col)
                                       | (static_cast<uint32_t>(row) << 16);
                    auto bit = ctx.bouncingQBlocks->find(key);
                    if (bit != ctx.bouncingQBlocks->end()) {
                        constexpr float BOUNCE_DURATION = 0.22f;
                        constexpr float BOUNCE_HEIGHT   = 10.f;
                        const float t = bit->second / BOUNCE_DURATION; // 0→1
                        const float offsetY = -BOUNCE_HEIGHT * std::sin(t * 3.14159f) * z;
                        dest.y += offsetY;
                    }
                }
                if (ctx.questionTile) {
                    SDL_RenderTexture(ctx.renderer, ctx.questionTile, nullptr, &dest);
                } else {
                    SDL_SetRenderDrawColor(ctx.renderer, 255, 200, 30, 255);
                    SDL_RenderFillRect(ctx.renderer, &dest);
                }
                continue;
            }

            if (cell == TileCell::Coin)
                continue; // coin entities are spawned separately

            if (cell == TileCell::Dirt)
                src = &dirtSrc;

            SDL_RenderTexture(ctx.renderer, ctx.terrainTile, src, &dest);

            if (cell == TileCell::Finish) {
                const bool light = ((col + row) & 1) == 0;
                SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ctx.renderer,
                                       light ? 255 : 220,
                                       light ? 255 : 50,
                                       light ? 255 : 50,
                                       160);
                SDL_RenderFillRect(ctx.renderer, &dest);
            }
        }
    }
}

} // namespace

// ──────────────────────────────────────────────────────────────
void render_system(const SystemContext& ctx)
{
    static const Mask mask = MaskBuilder()
        .set<TransformComponent>()
        .set<DrawableComponent>()
        .build();
    static int q = World::createQuery(mask);

    SDL_FPoint camPos{0.f, 0.f};
    if (ctx.camera.has<TransformComponent>())
        camPos = ctx.camera.get<TransformComponent>().position;

    const float z = ctx.zoom;

    if (ctx.mapTexture) {
        SDL_SetRenderDrawColor(ctx.renderer, 100, 180, 240, 255);
        SDL_RenderClear(ctx.renderer);
        SDL_FRect mapDest = {
            -camPos.x * z,
            -camPos.y * z,
            ctx.mapWidthPx * z,
            ctx.mapHeightPx * z
        };
        SDL_RenderTexture(ctx.renderer, ctx.mapTexture, nullptr, &mapDest);
    } else {
        SDL_SetRenderDrawColor(ctx.renderer, 100, 180, 240, 255);
        SDL_RenderClear(ctx.renderer);
    }

    renderTileMap(ctx, camPos, z);

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
                anim.frame_index = (anim.frame_index + 1) % SPRITE_RUN_FRAMES;
            }
            const int col = anim.frame_index % SPRITE_COLS;
            src = {
                col * SPRITE_FRAME_W + SPRITE_CROP_X,
                SPRITE_CROP_Y,
                SPRITE_CROP_W,
                SPRITE_CROP_H
            };
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

    // Score HUD (top-left).
    {
        static const Mask playerMask = MaskBuilder().set<PlayerStateComponent>().build();
        static int playerQ = World::createQuery(playerMask);

        for (Entity e = World::first(playerQ); !World::eof(playerQ); e = World::next(playerQ)) {
            const int score = e.get<PlayerStateComponent>().score;
            char      buf[32];
            std::snprintf(buf, sizeof(buf), "Score: %d", score);
            SDL_SetRenderDrawColor(ctx.renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(ctx.renderer, 16.f, 12.f, buf);
            break;
        }
    }

    // Win overlay when any player has crossed the finish line.
    {
        static const Mask playerMask = MaskBuilder().set<PlayerStateComponent>().build();
        static int playerQ = World::createQuery(playerMask);

        bool showWin = false;
        for (Entity e = World::first(playerQ); !World::eof(playerQ); e = World::next(playerQ)) {
            if (e.get<PlayerStateComponent>().has_finished) {
                showWin = true;
                break;
            }
        }

        if (showWin) {
            SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 160);
            SDL_FRect overlay{ 0.f, 0.f,
                               static_cast<float>(SCREEN_W),
                               static_cast<float>(SCREEN_H) };
            SDL_RenderFillRect(ctx.renderer, &overlay);
            SDL_SetRenderDrawColor(ctx.renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(ctx.renderer, 520.f, 320.f, "YOU WIN!");
            SDL_RenderDebugText(ctx.renderer, 460.f, 360.f, "Press R to restart");

            for (Entity e = World::first(playerQ); !World::eof(playerQ); e = World::next(playerQ)) {
                const int score = e.get<PlayerStateComponent>().score;
                char      buf[48];
                std::snprintf(buf, sizeof(buf), "Final score: %d", score);
                SDL_RenderDebugText(ctx.renderer, 480.f, 400.f, buf);
                break;
            }
        }
    }

    SDL_RenderPresent(ctx.renderer);
}
