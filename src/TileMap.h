#pragma once

#include <cstdint>
#include <vector>

enum class TileCell : uint8_t {
    Empty = 0,
    Grass,
    Dirt,
    Finish,        // checkered strip — solid like grass
    Cloud,         // decorative, no collision
    Coin,          // marks a coin spawn, no solid
    QuestionBlock, // solid powerup block
};

struct TileMap {
    int                   cols  = 0;
    int                   rows  = 0;
    std::vector<TileCell> cells;

    TileCell at(int col, int row) const
    {
        if (col < 0 || row < 0 || col >= cols || row >= rows)
            return TileCell::Empty;
        return cells[static_cast<std::size_t>(row * cols + col)];
    }

    void set(int col, int row, TileCell cell)
    {
        if (col < 0 || row < 0 || col >= cols || row >= rows)
            return;
        cells[static_cast<std::size_t>(row * cols + col)] = cell;
    }
};
