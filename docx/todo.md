# Fun Run — Task List

> Codebase: C++20, bagel26 ECS engine, SDL3 rendering, Box2D physics.
> Key files: `src/Game.cpp`, `src/Systems.cpp`, `src/EntityFactory.cpp`, `src/Components.h`, `src/Game.h`.

---

## 1. Fix Spam Jump ✅

> **Done.** `jump_lock_frames` on `PhysicsBodyComponent`; controller gates on
> `is_grounded && jump_lock_frames == 0`; physics decrements the lock each frame.
> Grounded uses Box2D contact normals (better than the velocity window in the
> original spec).

**Problem:** The player can jump multiple times without landing. The grounded check in
`physics_system` (`Systems.cpp:103`) uses a velocity threshold — when the player is at
the apex of a jump, `vel.y ≈ 0`, which falsely signals "grounded" and allows a second
jump on the same press.

**What to do:**

1. Add an `int jump_lock_frames` field to `PhysicsBodyComponent` in `Components.h`.
2. In `controller_system` (`Systems.cpp`): only allow a jump when both `is_grounded`
   AND `jump_lock_frames == 0`. On jump, set `jump_lock_frames = 18` (≈ 0.3 s at 60 fps).
3. In `physics_system` (`Systems.cpp`): after each step, decrement `jump_lock_frames`
   if it is > 0.
4. Tighten the grounded velocity window from `vel.y < 2.0f` to `vel.y <= 0.5f` so
   slow falls no longer count as grounded.
5. Update every `PhysicsBodyComponent{body, shape, false}` initializer in
   `EntityFactory.cpp` to include the new field: `{body, shape, false, 0}`.

**Files:** `src/Components.h`, `src/Systems.cpp`, `src/EntityFactory.cpp`

---

## 2. Make the Player Smaller ✅

> **Done.** `PLAYER_DRAW_W/H = 56.f` in `EntityFactory.cpp`.

**Problem:** The player is drawn at 80×80 px which is too large relative to the platforms.

**What to do:**

Change the two constants at the top of `EntityFactory.cpp`:
```cpp
static constexpr float PLAYER_DRAW_W = 80.f;  // → change to 56.f
static constexpr float PLAYER_DRAW_H = 80.f;  // → change to 56.f
```
The physics body size (32×48 px) does not need to change — it stays in sync via the
bottom-align logic in `render_system` (`PLAYER_BODY_HH = 24.f`).

**Files:** `src/EntityFactory.cpp`

---

## 3. Fix the PNG Spritesheet ✅

> **Done.** `1774.f / 4.f` and `887.f / 2.f` in both `EntityFactory.cpp` and `Systems.cpp`.

**Problem:** The spritesheet (`res/spritesheet.png`) is **1774×887 px**. The code hardcodes
frame dimensions as `443×443`, but the real frame size is `443.5×443.5` (1774÷4, 887÷2).
This causes a tiny misalignment on the last column/row that can show a sliver of the
adjacent frame.

**What to do:**

In both `EntityFactory.cpp` and `Systems.cpp`, replace the hardcoded constants with the
exact division:
```cpp
// Before
static constexpr float SPRITE_FRAME_W = 443.f;
static constexpr float SPRITE_FRAME_H = 443.f;

// After
static constexpr float SPRITE_FRAME_W = 1774.f / 4.f;   // = 443.5
static constexpr float SPRITE_FRAME_H =  887.f / 2.f;   // = 443.5
```
SDL3 src rects are floats, so the half-pixel value is valid.

**Files:** `src/EntityFactory.cpp`, `src/Systems.cpp`

---

## 4. Camera Zoom In ✅

> **Done.** `CAMERA_ZOOM = 1.5f`, `SystemContext::zoom`, camera + render scaling,
> `GROUND_Y = 370`.

**Problem:** The camera renders the world at 1:1 scale. Everything looks small on a
1280×720 screen. We want the world to appear ≈1.5× bigger (zoomed in).

**How zoom works in this engine:** The camera tracks the leading player's X position.
The `render_system` computes each entity's screen position as
`screenX = worldX − cameraX`. With zoom, it becomes
`screenX = (worldX − cameraX) × zoom`. The visible world width shrinks to
`SCREEN_W ÷ zoom`.

**What to do:**

1. Add a constant to `Game.h`:
   ```cpp
   inline constexpr float CAMERA_ZOOM = 1.5f;
   ```

2. Add a `float zoom` field to `SystemContext` in `Systems.h`, and pass `CAMERA_ZOOM`
   when constructing it in `Game::run()` (`Game.cpp`).

3. In `camera_system` (`Systems.cpp`): use `SCREEN_W / ctx.zoom` as the visible world
   width when centering on the player and when clamping to the level edges.

4. In `render_system` (`Systems.cpp`): multiply all screen-space positions and sizes by
   `ctx.zoom`:
   ```cpp
   const float z = ctx.zoom;
   dest.x = (worldLeft - camPos.x) * z;
   dest.y = (worldTop  - camPos.y) * z;
   dest.w = baseW * z;
   dest.h = baseH * z;
   ```

   **Important:** At 1.5× zoom the visible world height is `720 ÷ 1.5 = 480 px`.
   Make sure `GROUND_Y` in `Game.cpp` is ≤ 370 so the ground renders at
   `370 × 1.5 = 555 px` (lower third of the screen), which looks natural.
   If `GROUND_Y` stays at 580 the ground will be invisible (580 > 480).

**Files:** `src/Game.h`, `src/Systems.h`, `src/Systems.cpp`, `src/Game.cpp`

---

## 5. Build the Map — Platforms & Style ✅

> **Done.** 5-section level in `Game.cpp` (`LEVEL_W = 5000`, steel-blue platforms,
> brown ground).

**Problem:** The current level is a simple row of 10 identically-spaced platforms. It needs
a designed layout with clear sections of increasing difficulty, visual variety, and room to
run.

**Key physics numbers:**
- Player run speed: `2.5 m/s × 50 px/m = 125 px/s`
- Max jump height: `≈ 184 px` above the current surface
- Max horizontal jump distance: `≈ 216 px`
- Platform positions in `Game.cpp` are **body centers** — top surface = `center.y − height/2`

**Level design (5 sections, LEVEL_W = 5000 px, GROUND_Y = 370):**

| Section | X range | Description | Platform tops |
|---------|---------|-------------|---------------|
| 1 | 300–1300 | Easy warm-up stairs | 320–355 px (low) |
| 2 | 1300–2300 | Ascending, real jumps needed | 240–280 px |
| 3 | 2300–3200 | Wide platforms at altitude | 220–250 px |
| 4 | 3200–4300 | Precision — narrow (110–130 px wide) | 215–260 px |
| 5 | 4300–5000 | Staircase back to ground | 280–330 px |

Gap sizes: Section 1 ≈ 70–80 px · Section 2 ≈ 85–95 px · Section 3 ≈ 80–110 px · Section 4 ≈ 55–70 px

**Styling — platform textures:**

Platforms currently use a 2×2 solid-colour texture created in `Game.cpp` via
`makeSolidTex()`. Change the colour to something that reads clearly:
```cpp
_platTexture   = makeSolidTex(_renderer,  70, 130, 200);  // steel blue
_groundTexture = makeSolidTex(_renderer, 100,  70,  40);  // brown dirt
```

To add real platform sprites later, replace `_platTexture` with a loaded
`SDL_Texture*` from `res/` and pass it into `createPlatform`. No other code needs
to change — `createPlatform` already accepts any texture.

**Files:** `src/Game.cpp`

---

## 6. Add Start Line and Finish Line ✅

> **Done.** `_startTex` / `_finishTex`, `createDecoration()`, poles at x=150 and
> x=LEVEL_W−150.

**Problem:** There is no visual indicator of where the race begins or ends.

**What to do:**

1. Add two new solid-colour textures in `Game.h` / `Game.cpp`:
   ```cpp
   SDL_Texture* _startTex  = nullptr;  // bright green
   SDL_Texture* _finishTex = nullptr;  // red
   ```
   Initialise them in `Game::Game()` with `makeSolidTex`, and destroy them in `~Game()`.

2. Add a `createDecoration(SDL_Texture*, SDL_FPoint pos, float w, float h)` factory to
   `EntityFactory.cpp` / `.h`. It creates an entity with only
   `TransformComponent + DrawableComponent` — **no Box2D body**, purely visual.

3. In `Game::init_world()`, place the poles:
   ```cpp
   // Pole: 10 px wide, 310 px tall (from near top of screen down to ground)
   constexpr float POLE_W  = 10.f;
   constexpr float POLE_H  = GROUND_Y - 60.f;       // 310 px
   constexpr float POLE_CY = 60.f + POLE_H / 2.f;   // body center Y

   createDecoration(_startTex,  {  150.f,          POLE_CY }, POLE_W, POLE_H);
   createDecoration(_finishTex, { LEVEL_W - 150.f, POLE_CY }, POLE_W, POLE_H);
   ```

4. In `render_system`, decoration entities have no `AnimationComponent`, so they
   already use the center-align path — no extra code needed.

**Note:** These are cosmetic only. To actually detect the player crossing the finish line,
a Box2D sensor shape + `SensorAreaComponent` needs to be added (separate task).

**Files:** `src/Game.h`, `src/Game.cpp`, `src/EntityFactory.h`, `src/EntityFactory.cpp`

---

## Remaining / Future Tasks

| Task | Notes |
|------|-------|
| Finish line triggers end of round | ✅ Sensor + `sensor_system` + win overlay + R to restart |
| Grounded via Box2D contacts | Replace velocity heuristic with `b2Body_GetContactData` — more reliable on slopes |
| Player falls off screen → respawn | Add a kill-plane (y > GROUND_Y + 200) check in a new system |
| 2–4 local players | Extend `createPlayer` with keyboard mappings; camera tracks the leader |
| Items / traps | `createItemBox` sensor shape is stubbed; wire `b2SensorEvents` → `SensorAreaComponent` |
| Platform sprite variety | Load wood / stone / ice textures from `res/` and assign per-section |
| Sound effects | SDL3 audio — jump, land, item pickup, finish fanfare |
