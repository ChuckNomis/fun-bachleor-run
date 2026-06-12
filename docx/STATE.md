# Fun Run Demo — Project State

## What This Is
A playable demo of a Fun Run-style auto-runner platformer built with:
- **bagel26** — custom header-only ECS engine (`lib/bagel26/bagel.h`)
- **SDL3** — window, rendering, input
- **SDL3_image** — PNG loading (spritesheet, tile atlas)
- **Box2D 3.x** — 2D physics (gravity, capsule player, static terrain bodies)

---

## Current State

The game **builds and runs** as a single-player auto-runner. The player auto-runs right,
jumps with SPACE/W/UP, and races to a finish line. Features implemented:

- Tile-based level (100×20 grid, 6400×1280 px) defined as ASCII in `LevelBuilder.cpp`
- Grass, Dirt, Cloud, and Finish tiles rendered from a tile atlas (`res/terrain.png`)
- Q-block tiles (animated ? block from `res/tiles.png`) that bounce and disappear when hit from below
- Coin entities that are collected on contact, with a score HUD
- Finish line sensor with win overlay and R to restart
- Coyote-time, jump buffering, no double-jump
- Speed acceleration: multiplier grows at 0.3×/sec (cap 3×), resets on wall collision
- Visual level designer at `level-designer.html` (exports kRows[] for paste into LevelBuilder.cpp)

---

## Key Constants (`src/Game.h`)

| Constant | Value | Purpose |
|---|---|---|
| `BOX_SCALE` | 50.f | Pixels per Box2D meter |
| `SCREEN_W/H` | 1280×720 | Window size |
| `TARGET_FPS` | 60 | Frame cap |
| `CAMERA_ZOOM` | 1.0f | Render scale (1 = no zoom) |
| `PHYS_CAT_DEFAULT` | 0x0001 | Collision category for player + terrain |
| `PHYS_CAT_QBLOCK` | 0x0002 | Collision category for Q-block bodies (player mask excludes this) |

---

## Systems Pipeline

`input → controller → physics → sensor → damage → qblock → camera → render`

| System | Key behaviour |
|---|---|
| `input_system` | Auto-sets move_right; edge-detects jump key with buffer |
| `controller_system` | Sets vel.x = RUN_SPEED × speed_multiplier; grows multiplier each frame, resets on wall hit (contact normal n.x > 0.5); handles jump with coyote frames |
| `physics_system` | Steps Box2D; syncs TransformComponent; updates is_grounded via AABB probe |
| `sensor_system` | Resolves finish-line and coin sensor events |
| `damage_system` | Scaffolded (no active traps) |
| `qblock_system` | Detects head-hit from below (vel.y < 0, probe 6 px above capsule top, ±14 px horizontal); destroys Box2D body + ECS entity; starts bounce timer |
| `camera_system` | Tracks lead player; clamps to level bounds |
| `render_system` | Clears sky-blue; calls renderTileMap; draws all Transform+Drawable entities; score/win HUD |

---

## Components (`src/Components.h`)

| Component | Notable fields |
|---|---|
| `TransformComponent` | position (SDL_FPoint), rotation_degrees |
| `DrawableComponent` | texture, src_rect, dest_dimensions, flip_flags |
| `PhysicsBodyComponent` | body_id, shape_id, is_grounded, jump_lock_frames, coyote_frames |
| `PlayerInputComponent` | move_right/left, jump_pressed, jump_buffer_frames |
| `PlayerStateComponent` | lives, **current_speed_multiplier**, powerup_id, is_eliminated, has_finished, score |
| `GravityShiftComponent` | is_inverted, cooldown_timer_ms, max_shift_duration_ms |
| `SensorAreaComponent` | shape_id, sensor_type_id, is_triggered_this_frame |
| `TrapComponent` | speed_penalty_factor, stun_duration_seconds, owner_entity_id |
| `AnimationComponent` | frame_index, timer_ms, frame_duration_ms |
| `CoinComponent` | value |

---

## Physics Layout

- **Player**: Box2D dynamic capsule (radius 15 px, centers ±21 px), `motionLocks.angularZ = true`, friction = 0
- **Terrain (Grass/Dirt/Finish/Cloud)**: Merged into wide static bodies per row for efficiency
- **Q-blocks**: Individual static bodies per tile, category=`PHYS_CAT_QBLOCK`, maskBits=0 → no physical collision with anything. Player mask excludes PHYS_CAT_QBLOCK → player passes through Q-blocks in all directions. Hit detection is position-based.
- **Coins**: Static sensor bodies (SensorType::Coin)
- **Finish line**: Static sensor body (SensorType::FinishLine)

---

## Q-Block System Details

- **Tile**: `TileCell::QuestionBlock`, rendered from `res/tiles.png` at crop (sx=1032, sy=1584, 256×256)
- **Hit detection**: `qblock_system` probes 6 px above player head, ±14 px wide. Triggers only when `vel.y < 0` (moving up).
- **On hit**: Physics body + ECS entity destroyed immediately; bounce timer started (key = col | row<<16 in `_bouncingQBlocks`)
- **Bounce**: 0.22s duration, 10 px peak, `sin(t*π)` Y-offset applied in `renderTileMap`
- **After bounce**: Tile set to `TileCell::Empty` via `TileMap::set()`

---

## Speed Acceleration

- `PlayerStateComponent::current_speed_multiplier` starts at 1.0 (set in EntityFactory)
- `controller_system` grows it by `0.3 × frameDtSec` each frame, capped at 3.0×
- Wall contact (contact normal n.x > 0.5 from player's perspective) resets it to 1.0
- Effective speed range: 2.5 m/s (base) → 7.5 m/s (3× cap, reached after ~7 seconds of open running)

---

## Level Design

- **Grid**: 100 cols × 20 rows, tile size = 64 px
- **Map string** in `LevelBuilder.cpp` (`kRows[]`): `.GDFCOq` chars → TileCell enum
- **Tile chars**: `.`=Empty, `G`=Grass, `D`=Dirt, `F`=Finish, `C`=Cloud, `O`=Coin, `Q`=QuestionBlock
- **Coin spawns**: auto above floating platforms + hardcoded ground positions + explicit O tiles
- **level-designer.html**: Visual grid editor at project root; exports kRows[] to paste into LevelBuilder.cpp

---

## Asset Pipeline

| Asset | Path | Usage |
|---|---|---|
| Player spritesheet | `res/spritesheet.png` | 4-col animated run cycle, checkerboard background stripped |
| Tile atlas | `res/tiles.png` | Q-block crop at (1032, 1584, 256×256); finish sign |
| Terrain texture | `res/terrain.png` (TERRAIN_TILE_PATH) | Ground/grass/dirt tiles |
| Coin | `res/coin.png` | White background stripped |

---

## Known Issues / Future Work

- [ ] Gravity shift mechanic (GravityShiftComponent scaffolded, not triggered)
- [ ] Multiple players / networked multiplayer
- [ ] Race reset doesn't rebuild Q-blocks (consumed blocks stay gone after R)
- [ ] Sound / music
- [ ] Items / powerups
