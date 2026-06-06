#pragma once

#include <cstdint>
#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include "bagel.h"

// ─────────────────────────────────────────────
// Component structs (pure data, no logic)
// Field lists per the Fun Run ECS Architecture & Implementation Plan
// ─────────────────────────────────────────────

using TransformComponent = struct {
    SDL_FPoint position;
    float      rotation_degrees;
};

using DrawableComponent = struct {
    SDL_Texture* texture;
    SDL_FRect    src_rect;
    SDL_FRect    dest_dimensions;
    int          flip_flags;
};

using PhysicsBodyComponent = struct {
    b2BodyId  body_id;
    b2ShapeId shape_id;
    bool      is_grounded;
};

using PlayerInputComponent = struct {
    bool move_right;
    bool move_left;
    bool jump_pressed;
    bool use_item_pressed;
    bool gravity_shift_pressed;
};

using PlayerStateComponent = struct {
    int   lives;
    float current_speed_multiplier;
    int   current_powerup_id;
    bool  is_eliminated;
};

using GravityShiftComponent = struct {
    bool  is_inverted;
    float cooldown_timer_ms;
    float maximum_shift_duration_ms;
};

using SensorAreaComponent = struct {
    int  sensor_type_id;
    bool is_triggered_this_frame;
};

using TrapComponent = struct {
    float    speed_penalty_factor;
    float    stun_duration_seconds;
    uint64_t owner_entity_id;
};

// ─────────────────────────────────────────────
// Storage overrides — must be visible before first use.
// PackedStorage is for components iterated every frame by hot systems.
// ─────────────────────────────────────────────

template <> struct bagel::Storage<PlayerInputComponent> final : bagel::NoInstance {
    using type = bagel::PackedStorage<PlayerInputComponent>;
};
template <> struct bagel::Storage<PlayerStateComponent> final : bagel::NoInstance {
    using type = bagel::PackedStorage<PlayerStateComponent>;
};
template <> struct bagel::Storage<GravityShiftComponent> final : bagel::NoInstance {
    using type = bagel::PackedStorage<GravityShiftComponent>;
};
template <> struct bagel::Storage<PhysicsBodyComponent> final : bagel::NoInstance {
    using type = bagel::PackedStorage<PhysicsBodyComponent>;
};
