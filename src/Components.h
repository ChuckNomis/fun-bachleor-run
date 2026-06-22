#pragma once

#include <cstdint>
#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include "bagel.h"

// ─────────────────────────────────────────────
// Component structs (pure data, no logic)
// IMPORTANT: must be named structs, not anonymous-struct type aliases.
// Anonymous structs (using Foo = struct {...}) have no linkage, so each
// translation unit gets a distinct type — Component<T>::Index would differ
// per TU, causing mask mismatches between addComponent and has<>/query.
// ─────────────────────────────────────────────

struct TransformComponent {
    SDL_FPoint position;
    float      rotation_degrees;
};

struct DrawableComponent {
    SDL_Texture* texture;
    SDL_FRect    src_rect;
    SDL_FRect    dest_dimensions;
    int          flip_flags;
    SDL_Color    color_mod = {255, 255, 255, 255};
};

struct PhysicsBodyComponent {
    b2BodyId  body_id;
    b2ShapeId shape_id;
    bool      is_grounded;
    int       jump_lock_frames;  // frames remaining before next jump is allowed
    int       coyote_frames;     // grace period after leaving ground
};

struct PlayerInputComponent {
    int  player_index;
    bool move_right;
    bool move_left;
    bool jump_pressed;
    bool use_item_pressed;
    bool gravity_shift_pressed;
    int  jump_buffer_frames; // remember jump input for a few frames
};

struct PlayerStateComponent {
    int   lives;
    float current_speed_multiplier;
    int   current_powerup_id;
    bool  is_eliminated;
    bool  has_finished; // crossed the finish-line sensor
    int   score;
};

struct CoinComponent {
    int value;
};

// Sensor type ids for SensorAreaComponent::sensor_type_id
namespace SensorType {
    inline constexpr int FinishLine = 1;
    inline constexpr int ItemBox    = 2;
    inline constexpr int Coin         = 3;
}

struct GravityShiftComponent {
    bool  is_inverted;
    float cooldown_timer_ms;
    float maximum_shift_duration_ms;
};

struct SensorAreaComponent {
    b2ShapeId shape_id;
    int       sensor_type_id;
    bool      is_triggered_this_frame;
};

struct TrapComponent {
    float    speed_penalty_factor;
    float    stun_duration_seconds;
    uint64_t owner_entity_id;
};

struct AnimationComponent {
    int   frame_index;
    float timer_ms;
    float frame_duration_ms;
};

struct BgmComponent {
    SDL_AudioStream* stream;
    Uint8*           audio_buf;
    Uint32           audio_len;
    bool             is_playing;
};

struct CameraComponent {
    int      target_player_index;
    SDL_Rect viewport_rect;
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
template <> struct bagel::Storage<AnimationComponent> final : bagel::NoInstance {
    using type = bagel::PackedStorage<AnimationComponent>;
};
template <> struct bagel::Storage<CameraComponent> final : bagel::NoInstance {
    using type = bagel::PackedStorage<CameraComponent>;
};
