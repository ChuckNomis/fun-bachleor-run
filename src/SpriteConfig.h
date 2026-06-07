#pragma once

// Spritesheet: 1774×887 px, 4 cols × 2 rows (row 1 duplicates row 0).
inline constexpr float SPRITE_SHEET_W = 1774.f;
inline constexpr float SPRITE_SHEET_H = 887.f;
inline constexpr int   SPRITE_COLS    = 4;
inline constexpr int   SPRITE_RUN_FRAMES = 4; // use top row only

inline constexpr float SPRITE_FRAME_W = SPRITE_SHEET_W / 4.f;
inline constexpr float SPRITE_FRAME_H = SPRITE_SHEET_H / 2.f;

// Crop inside each cell — trims checkerboard padding around the character.
inline constexpr float SPRITE_CROP_X = 70.f;
inline constexpr float SPRITE_CROP_Y = 45.f;
inline constexpr float SPRITE_CROP_W = 280.f;
inline constexpr float SPRITE_CROP_H = 360.f;
