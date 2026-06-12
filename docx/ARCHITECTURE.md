# Fun Run — Architecture

"Fun Run" is a single-player (multiplayer-ready) 2D racing platformer built on **bagel26**, a custom
header-only C++20 ECS engine (`lib/bagel26/bagel.h`), using **SDL3** for
windowing/input/rendering, **SDL3_image** for textures, and **Box2D 3.x** for physics.

---

## bagel26 ECS Fundamentals

- **Entities** are lightweight integer IDs (`bagel::Entity`). Create with `Entity::create()`, destroy with `.destroy()`.
- **Components** are plain named structs (anonymous structs break cross-TU type identity — see Components.h).
- **Storage backends**: `SparseStorage<T>` (default), `PackedStorage<T>` (dense, cache-friendly, for hot components), `StackStorage<T>`, `TaggedStorage<T>`. Override per-type with `template<> struct bagel::Storage<T>`.
- **Masks & queries**: `MaskBuilder().set<A>().set<B>().build()` → `World::createQuery(mask)`. Iterate with `World::first(q)` / `World::next(q)` / `World::eof(q)`. Queries are persistent and safe to create as `static int q`.
- **MaxComponents = 16** (bumped from shipped 6 to cover current 10 components).

---

## Components (`src/Components.h`)

| Component | Fields | Storage |
|---|---|---|
| `TransformComponent` | `position` (SDL_FPoint), `rotation_degrees` | Sparse |
| `DrawableComponent` | `texture`, `src_rect`, `dest_dimensions`, `flip_flags` | Sparse |
| `PhysicsBodyComponent` | `body_id`, `shape_id`, `is_grounded`, `jump_lock_frames`, `coyote_frames` | **Packed** |
| `PlayerInputComponent` | `move_right/left`, `jump_pressed`, `use_item_pressed`, `gravity_shift_pressed`, `jump_buffer_frames` | **Packed** |
| `PlayerStateComponent` | `lives`, `current_speed_multiplier`, `current_powerup_id`, `is_eliminated`, `has_finished`, `score` | **Packed** |
| `GravityShiftComponent` | `is_inverted`, `cooldown_timer_ms`, `maximum_shift_duration_ms` | **Packed** |
| `SensorAreaComponent` | `shape_id`, `sensor_type_id`, `is_triggered_this_frame` | Sparse |
| `TrapComponent` | `speed_penalty_factor`, `stun_duration_seconds`, `owner_entity_id` | Sparse |
| `AnimationComponent` | `frame_index`, `timer_ms`, `frame_duration_ms` | **Packed** |
| `CoinComponent` | `value` | Sparse |

---

## Entity Blueprints (`src/EntityFactory.{h,cpp}`)

| Factory function | Components | Box2D body |
|---|---|---|
| `createPlayer` | Transform + Drawable + PhysicsBody + PlayerInput + PlayerState + GravityShift + Animation | Dynamic capsule; `motionLocks.angularZ=true`; friction=0; filter `categoryBits=PHYS_CAT_DEFAULT, maskBits=PHYS_CAT_DEFAULT` |
| `createPlatform` | Transform + Drawable + PhysicsBody | Static box |
| `createPhysicsPlatform` | Transform + PhysicsBody | Static box (no Drawable; used for terrain and Q-blocks) |
| `createSensorArea` | Transform + (Drawable?) + SensorArea | Static sensor |
| `createCoin` | (via createSensorArea) + Coin | Static sensor, type=SensorType::Coin |
| `createCamera` | Transform only | None |
| `createProjectile` | Transform + Drawable + PhysicsBody + Trap | Dynamic circle |

---

## Systems (`src/Systems.{h,cpp}`)

Systems run in this order each frame (`Game::run`):

```
input → controller → physics → sensor → damage → qblock → camera → render
```

### `input_system`
Polls `SDL_GetKeyboardState` + event queue. Sets `move_right=true` always (auto-run). Edge-detects jump key (SPACE/W/UP) with a `JUMP_BUFFER_FRAMES=10` buffer stored in `PlayerInputComponent`.

### `controller_system`
- **Speed acceleration**: reads `PlayerStateComponent::current_speed_multiplier`, grows by `ACCEL_PER_SEC=0.3` per second (cap `MAX_SPEED_MULT=3.0`). Detects wall contacts via `b2Body_GetContactData` — any contact with `n.x > 0.5` (rightward normal from player's POV) resets multiplier to 1.0.
- Sets `vel.x = RUN_SPEED_MPS × multiplier` via `b2Body_SetLinearVelocity`.
- Handles jump: coyote frames + jump lock prevent double-jump and spam.

### `physics_system`
Steps Box2D (`b2World_Step`). Syncs `TransformComponent` from body transform (m→px via `BOX_SCALE=50`). Updates `is_grounded` via AABB overlap probe below the player's feet + contact manifold fallback.

### `sensor_system`
Reads `b2World_GetSensorEvents`. Resolves:
- `SensorType::FinishLine` → sets `has_finished`, stops player
- `SensorType::Coin` → adds to score, destroys coin body + entity

### `damage_system`
Scaffolded for trap/hazard logic. Currently no-op.

### `qblock_system`
Detects player head hits on Q-block tiles and manages bounce animation:
1. **Advance bounces**: increments elapsed time in `_bouncingQBlocks` map; on completion calls `TileMap::set(col, row, Empty)`.
2. **Detect new hits**: for each player with `vel.y < 0`, probes 6 px above head ±14 px wide; checks `TileMap::at()` for `QuestionBlock`; on match — destroys the Q-block's Box2D body and ECS entity (from `_qBlockBodies` map), starts bounce timer.

### `camera_system`
Finds the lead non-eliminated player; centers camera on them; clamps to level bounds.

### `render_system`
- Clears to sky-blue.
- Calls `renderTileMap`: iterates visible tile columns/rows, renders terrain textures, cloud shapes (overlapping rects), Q-block sprites with bounce Y-offset (`sin(t×π) × 10px`), finish tile checkerboard overlay.
- Draws all `Transform+Drawable` entities (coins, player sprite with animation and bottom-align).
- Score HUD (top-left). Win overlay on finish.

---

## Tile Map (`src/TileMap.h`, `src/LevelBuilder.{h,cpp}`)

`TileMap` is a flat `vector<TileCell>` (row-major). Tile types:

| Char | TileCell | Physics | Rendered as |
|---|---|---|---|
| `.` | Empty | none | (nothing) |
| `G` | Grass | merged static body | terrain texture (full) |
| `D` | Dirt | merged static body | terrain texture (dirt strip) |
| `F` | Finish | merged static body | terrain + checkerboard overlay |
| `C` | Cloud | merged static body | white overlapping rects |
| `O` | Coin | none (spawn point) | coin entity |
| `Q` | QuestionBlock | individual static body (PHYS_CAT_QBLOCK, maskBits=0) | Q-block sprite from tiles.png |

**Merged-row physics**: `addMergedRowPhysics` scans each row and merges consecutive solid tiles into a single wide static body — efficient for terrain. Q-blocks are excluded from merging and get individual bodies so they can be destroyed independently.

**Q-block collision filtering**: Q-block bodies use `categoryBits=PHYS_CAT_QBLOCK, maskBits=0`. The player shape uses `categoryBits=PHYS_CAT_DEFAULT, maskBits=PHYS_CAT_DEFAULT`. Neither side includes the other's category → no physical collision from any direction. Head-hit detection is purely position-based (tilemap lookup), so it remains functional.

---

## Game State (`src/Game.{h,cpp}`)

`Game` owns:
- SDL window/renderer and all textures
- Box2D world
- `TileMap _tileMap` — mutable (Q-blocks are cleared via `set()`)
- `unordered_map<uint32_t, bagel::Entity> _qBlockBodies` — live Q-block entities keyed by `col | row<<16`
- `unordered_map<uint32_t, float> _bouncingQBlocks` — active bounce timers
- Level geometry (widthPx, heightPx, playerStart, finishSensor)
- Camera entity

`SystemContext` is constructed once before the game loop and passed to every system each frame. It contains pointers to the mutable maps and tileMap so systems can modify them without globals.

---

## Coordinate System

- **Box2D**: meters, +Y downward
- **Screen/Tile**: pixels, +Y downward
- **Conversion**: `BOX_SCALE = 50.f` px/m (÷ for pixel→Box2D, × for Box2D→pixel)
- **Tile size**: `TILE_SIZE = 64` px
- **Level**: 100×20 tiles = 6400×1280 px

---

## Directory Layout

```
src/               Game source
  Game.h/.cpp      Window, renderer, game loop, SystemContext
  Systems.h/.cpp   All systems + renderTileMap
  Components.h     All component structs + storage overrides
  EntityFactory.h/.cpp  Entity creation helpers
  LevelBuilder.h/.cpp   kRows[] map, physics build, coin spawns
  TileMap.h        TileCell enum, TileMap struct (at/set)
  TileConfig.h     Tile texture constants
  SpriteConfig.h   Spritesheet crop constants
res/               Runtime assets (copied next to binary by CMake)
lib/               bagel26, SDL3, SDL3_image, box2d (submodules)
docx/              Design documents and state
level-designer.html  Visual tile editor (exports kRows[])
```
