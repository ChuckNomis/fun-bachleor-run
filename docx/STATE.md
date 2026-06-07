# Fun Run Demo — Project State

## What This Is
A playable demo of a Fun Run-style auto-runner platformer built with:
- **bagel26** — custom header-only ECS engine (`lib/bagel26/bagel.h`)
- **SDL3** — window, rendering, input
- **SDL3_image** — PNG loading (spritesheet)
- **Box2D 3.x** — 2D physics (gravity, collisions, static bodies)

---

## Current State (as of this session)

The game **builds and runs**. You see:
- Sky-blue background
- Brown ground strip at the bottom
- 10 green platforms at varying heights across a 4000px wide level
- Player character (animated spritesheet) auto-runs right, jumps with SPACE

---

## Files Changed and Why

### `src/Components.h`
**Critical bug fix**: All component structs were written as anonymous-struct type aliases:
```cpp
using TransformComponent = struct { ... };  // WRONG
```
Anonymous structs have no C++ linkage — each translation unit gets a **different type**, so `Component<T>::Index` (the bit-mask counter) was assigned different values per TU. This caused entity masks set in `EntityFactory.cpp` to be unreadable in `Systems.cpp` → render query always returned 0 entities → player invisible.

**Fix**: Changed all to named structs:
```cpp
struct TransformComponent { ... };  // CORRECT — same type across all TUs
```

**Components defined** (9 total, within bagel26's MaxComponents=16 cap):
1. `TransformComponent` — position (SDL_FPoint) + rotation_degrees
2. `DrawableComponent` — texture, src_rect, dest_dimensions, flip_flags
3. `PhysicsBodyComponent` — b2BodyId, b2ShapeId, is_grounded (bool)
4. `PlayerInputComponent` — move_right/left, jump_pressed, use_item_pressed, gravity_shift_pressed
5. `PlayerStateComponent` — lives, speed_multiplier, powerup_id, is_eliminated
6. `GravityShiftComponent` — is_inverted, cooldown_timer_ms, max_shift_duration_ms
7. `SensorAreaComponent` — sensor_type_id, is_triggered_this_frame
8. `TrapComponent` — speed_penalty_factor, stun_duration_seconds, owner_entity_id
9. `AnimationComponent` — frame_index (0-7), timer_ms, frame_duration_ms

PackedStorage override applied to: PlayerInput, PlayerState, GravityShift, PhysicsBody, Animation.

---

### `src/Game.h`
- Constants: `BOX_SCALE=50` (px/meter), `SCREEN_W=1280`, `SCREEN_H=720`, `TARGET_FPS=60`
- Game class holds: window, renderer, spritesheet texture, platform texture, ground texture, Box2D world, running flag, camera entity

---

### `src/Game.cpp`
**Current level**: Simple flat level (no map.png), 4000px wide.

**Key constants**:
- `LEVEL_W = 4000.f`
- `GROUND_Y = 580.f` (top of ground surface)

**Texture creation**: `makeSolidTex(renderer, R, G, B)` creates a 2×2 render-target texture filled with a solid colour. Used for platforms (green) and ground (brown). Stretches to any dest rect at render time.

**Spritesheet**: loaded from `res/spritesheet.png` (1774×887, 4 cols × 2 rows = 8 frames of 443×443px each). `SDL_BLENDMODE_BLEND` set so transparent PNG background doesn't show as black/grey squares.

**Physics world**: gravity = `{0, 9.81}` (positive Y = downward).

**`init_world()`** creates:
- Camera entity (ID 0)
- Ground platform entity: 4000×140px, center at `(2000, GROUND_Y+70)` → top surface at y=580
- 10 floating platform entities: varying X (350–3500), Y tops (450–510), width 180–200px, height 20px
- Player entity: spawns at `(120, 400)`, falls to ground on first frame

**`SystemContext`** passed each frame: physicsWorld, renderer, spritesheet, camera, frameDt=1/60, mapTexture=nullptr, mapWidthPx=LEVEL_W, mapHeightPx=SCREEN_H

---

### `src/EntityFactory.cpp`
**`createPlayer()`**:
- Box2D dynamic body, `motionLocks.angularZ = true` (no tumbling)
- Physics box half-extents: 16×24 px (small hitbox)
- Spritesheet texture, `AnimationComponent{frame=0, timer=0, duration=80ms}`
- `PlayerStateComponent{lives=3, speed=1.0, powerup=-1, eliminated=false}`

**`createPlatform()`**:
- Box2D static body centered at `posPx`
- `DrawableComponent` with texture; src_rect set to `{0,0,w,h}` (overridden at render time — see Systems.cpp)
- Used for both ground and floating platforms

**`createCamera()`**: entity with only `TransformComponent`, no physics

**`makeStaticBox()`** (in Game.cpp, not EntityFactory): physics-only static box, no entity/drawable — kept but currently unused (replaced by createPlatform for visible geometry)

---

### `src/Systems.h` / `SystemContext`
```cpp
struct SystemContext {
    b2WorldId     physicsWorld;
    SDL_Renderer* renderer;
    SDL_Texture*  spritesheet;
    bagel::Entity camera;
    float         frameDtSec;
    SDL_Texture*  mapTexture;   // nullptr in simple level
    float         mapWidthPx;   // = LEVEL_W, used for camera right-edge clamp
    float         mapHeightPx;
};
```

---

### `src/Systems.cpp`

**`input_system`**:
- Sets `move_right = true` every frame (auto-run)
- Sets `jump_pressed = true` on `SDL_EVENT_KEY_DOWN` + `SCANCODE_SPACE` + `key.repeat == 0`
- `key.repeat == 0` check prevents OS key-repeat from spamming jumps

**`controller_system`**:
- `vel.x = RUN_SPEED_MPS = 2.5f` (m/s) always
- Jump: `if (jump_pressed && phys.is_grounded)` → `vel.y = -8.5f`, clears `is_grounded`
- Uses `b2Body_SetLinearVelocity` each frame (kinematic-style control)

**`physics_system`**:
- `b2World_Step(world, 1/60, 4)` each frame
- Syncs `TransformComponent` from Box2D body transform (meters→pixels via BOX_SCALE)
- Updates `phys.is_grounded = (vel.y > -0.5f && vel.y < 2.0f)` after step
  - Near-zero vertical velocity = settled on a surface

**`camera_system`**:
- Finds player (entity with Transform + PlayerState, not eliminated)
- `camX = playerX - SCREEN_W/2`, clamped to `[0, LEVEL_W - SCREEN_W]`

**`render_system`**:
- Clear to sky blue `(100, 180, 240)`
- Draw map texture if present (currently nullptr — skipped)
- For each entity with Transform + Drawable:
  - If has `AnimationComponent`: advance timer, compute frame src rect (col/row from frame_index), **bottom-align** sprite: `dest.y = tr.y + 24 - 80 - camY` so feet land exactly on physics body bottom
  - Else: center sprite on transform position
  - Src rect: `nullptr` for non-animated (stretches full 2×2 solid texture); `&src` for animated (selects spritesheet frame)

---

### `CMakeLists.txt`
- `target_include_directories`: adds `src`, `lib/bagel26`, `lib/SDL_image/include`
- SDL3: static (`SDL_STATIC ON`)
- SDL3_image: `set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)` before `add_subdirectory` — required because shared SDL3_image conflicts with static SDL3
- Link targets: `SDL3-static`, `SDL3_image-static`, `box2d`
- Post-build: copies `res/` directory next to the executable

---

## Known Bugs Fixed During This Session

| Bug | Root Cause | Fix |
|-----|-----------|-----|
| Player invisible | Anonymous struct components → different `Component<T>::Index` per TU → mask mismatch | Named structs in Components.h |
| Black/grey squares on sprite | PNG loaded without alpha blend mode | `SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND)` |
| Jump spam | SDL key-repeat events firing `SDL_EVENT_KEY_DOWN` repeatedly while holding SPACE | `ev.key.repeat == 0` check in input_system |
| Jump mid-air | Old gate `vel.y >= -JUMP_VEL_GATE` was true at jump apex (vel.y≈0) | `phys.is_grounded` flag updated in physics_system |
| SDL3_image link error | Shared SDL3_image incompatible with static SDL3 | `BUILD_SHARED_LIBS OFF` before SDL_image subdirectory |
| Box2D API errors | Box2D 3.x moved `friction` into `shapeDef.material.friction`; replaced `fixedRotation` with `motionLocks.angularZ` | Updated all call sites |
| Spritesheet include error | Missing `lib/SDL_image/include` in CMake include dirs | Added to `target_include_directories` |

---

## bagel26 Engine Key Facts

- **Entity IDs**: `_maxId` starts at -1; first entity = ID 0 (camera), second = ID 1 (player)
- **Query creation**: `World::createQuery(mask)` scans ALL existing entities at creation time (safe to call after entities exist)
- **`addComponent` + queries**: when adding a component, bagel pushes the entity into any query it newly matches — but only if the query was created first. Since all system queries use `static int q = World::createQuery(mask)` (initialized on first call, which is after init_world), entities are found via the scan, not the push.
- **MaxComponents = 16** (currently using 9)
- **SparseStorage** (default): array indexed by entity ID
- **PackedStorage**: dense array with id→comp mapping; used for hot-path components

---

## What Still Needs Work (Future Sessions)

- [ ] Add the actual map.png background with correct terrain physics
- [ ] Player sprite faces left when it should face right (check flip_flags)
- [ ] End of level / win condition
- [ ] Multiple players / AI runners
- [ ] Items / powerups
- [ ] Sound
