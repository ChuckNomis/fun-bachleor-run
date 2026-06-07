# Fun Run — Architecture

"Fun Run" is a multiplayer 2D racing platformer built on **bagel26**, a custom
header-only C++20 ECS engine (`lib/bagel26/bagel.h`), using **SDL3** for
windowing/input/rendering, **SDL3_image** for textures, and **Box2D** for
physics. Its unique gimmick is **Personal Gravity Shift**: each player can
independently invert their local gravity vector and run on the ceiling,
entirely independent of other players.

This document explains how the codebase maps onto the bagel26 ECS
fundamentals and the design spec in
`docx/Fun Run - ECS Architecture & Implementation Plan (1).pdf`.

## bagel26 ECS fundamentals (as implemented in this repo's `bagel.h`)

- **Entities** are lightweight integer IDs (`bagel::Entity`). Create with
  `Entity::create()`, destroy with `.destroy()`.
- **Components** are plain structs with no behavior — declared as
  `using X = struct {...};` and registered automatically the first time
  they're used with `add<T>()`. No base class or manual registration needed.
- **Storage backends**: `SparseStorage<T>` (default — array indexed by entity
  ID, good for by-ID access), `PackedStorage<T>` (dense array with O(1)
  removal, good for components iterated every frame), `StackStorage<T>`
  (packed + free-list, for frequent add/remove), `TaggedStorage<T>` (marker
  flags only, no data). Override per-type with
  `template<> struct bagel::Storage<T> { using type = bagel::PackedStorage<T>; };`
  *before* the type is first used — see `Components.h`.
- **Masks & queries**: `bagel::MaskBuilder().set<A>().set<B>().build()` builds
  a component-signature bitmask. `World::createQuery(mask)` registers a
  persistent query that the engine keeps up to date as components are
  added/removed; iterate with
  `for (Entity e = World::first(q); !World::eof(q); e = World::next(q))`.
  Queries are safe to create eagerly (e.g. in a `static int q = ...`) even
  before matching entities exist — they populate lazily.

### `MaxComponents` bump

`bagel.h` caps the number of distinct component types via
`MaxComponents` (it sizes the bitmask type and several fixed-size bags).
The shipped value was `6`, but Fun Run defines **8** component types, so
exceeding it would silently corrupt component masks. `MaxComponents` was
bumped to **16** in `lib/bagel26/bagel.h` to comfortably cover the current 8
components plus headroom for future additions.

## Components (`src/Components.h`)

All 8 components from the design spec, declared as plain data structs:

| Component | Fields | Storage |
|---|---|---|
| `TransformComponent` | `position`, `rotation_degrees` | Sparse (default) |
| `DrawableComponent` | `texture`, `src_rect`, `dest_dimensions`, `flip_flags` | Sparse (default) |
| `PhysicsBodyComponent` | `body_id`, `shape_id`, `is_grounded` | **Packed** |
| `PlayerInputComponent` | `move_right/left`, `jump_pressed`, `use_item_pressed`, `gravity_shift_pressed` | **Packed** |
| `PlayerStateComponent` | `lives`, `current_speed_multiplier`, `current_powerup_id`, `is_eliminated` | **Packed** |
| `GravityShiftComponent` | `is_inverted`, `cooldown_timer_ms`, `maximum_shift_duration_ms` | **Packed** |
| `SensorAreaComponent` | `sensor_type_id`, `is_triggered_this_frame` | Sparse (default) |
| `TrapComponent` | `speed_penalty_factor`, `stun_duration_seconds`, `owner_entity_id` | Sparse (default) |

Components iterated every frame by hot systems (physics, controller, damage,
camera) use `PackedStorage` for cache-friendly dense iteration; everything
else stays on the default `SparseStorage`.

## Entity blueprints (`src/EntityFactory.{h,cpp}`)

Each blueprint is a free function that creates an entity, attaches its
component set via `addAll(...)`, and (where relevant) creates the matching
Box2D body:

- **`createPlayer`** — Transform + Drawable + PhysicsBody (dynamic) +
  PlayerInput + PlayerState + GravityShift
- **`createPlatform`** — Transform + Drawable + PhysicsBody (static)
- **`createItemBox`** — Transform + Drawable + SensorArea (sensor trigger
  region; finish lines / item boxes)
- **`createProjectile`** — Transform + Drawable + PhysicsBody (dynamic) +
  Trap (hazards / thrown items)
- **`createCamera`** — Transform only (anchor for screen tracking)

## Systems (`src/Systems.{h,cpp}`)

Systems are plain functions that build a `static const Mask` and a persistent
`static int q = World::createQuery(mask)`, then iterate matching entities each
frame. They run once per frame, in this order (see `Game::run`):

1. **`input_system`** — polls SDL events, populates `PlayerInputComponent`
2. **`controller_system`** — reads input/state, drives Box2D velocities, and
   toggles gravity shift via `b2Body_SetGravityScale`
3. **`physics_system`** — `b2World_Step`, then syncs `b2BodyId` transforms
   back into `TransformComponent` (meters → pixels via `BOX_SCALE`)
4. **`sensor_system`** — reads Box2D sensor events, resolves item-box
   pickups/finish-line crossings, updates `PlayerStateComponent`
5. **`damage_system`** — processes trap/hazard overlaps, applies penalties,
   stuns, and elimination
6. **`camera_system`** — finds the lead non-eliminated racer and anchors the
   camera entity's `TransformComponent` to track them
7. **`render_system`** — clears the renderer, offsets sprites by camera
   position, draws every `Transform`+`Drawable` entity via
   `SDL_RenderTextureRotated`

Most system bodies are scaffolded with `TODO` comments describing the
gameplay logic to fill in; `physics_system` and `render_system` are fully
implemented since they're pure plumbing (Box2D sync / SDL draw).

## Coordinate convention — `BOX_SCALE`

Box2D works in meters; the screen works in pixels. `BOX_SCALE` (defined in
`Game.h`, currently `50.f`) converts between them: divide pixel coordinates
by `BOX_SCALE` before passing them to Box2D, and multiply Box2D positions by
`BOX_SCALE` when syncing back into `TransformComponent`.

## Game loop (`src/Game.{h,cpp}`, `src/main.cpp`)

`Game` owns the SDL window/renderer, the spritesheet texture, the Box2D
world, and the camera entity. `main.cpp` is a thin entry point that
constructs `Game` and calls `run()`. The loop:

1. Polls SDL events (also catching `QUIT` / `ESC`)
2. Runs the 7 systems in the order listed above
3. Caps frame rate to `TARGET_FPS` (60) via `SDL_GetTicks` / `SDL_Delay`

`init_world()` spawns the camera, a placeholder ground platform, and two test
players — a starting point to be replaced by real level loading.

## Directory layout

```
src/        Game source (.h/.cpp)
res/        Runtime assets (map.png, spritesheet.png) — copied next to the
            binary by CMake's post-build step so IMG_Load("res/...") resolves
lib/        Engine + dependencies (bagel26, SDL, SDL_image, box2d submodules)
docx/       Design documents
```
