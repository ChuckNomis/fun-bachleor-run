#include "LevelBuilder.h"
#include "Components.h"
#include "EntityFactory.h"
#include "TileConfig.h"
#include <bagel.h>
#include <vector>

namespace {

enum CharCell : char {
    CEmpty  = '.',
    CGrass  = 'G',
    CDirt   = 'D',
    CFinish = 'F',
};

constexpr int kGroundRow = 7;
constexpr int kFinishCol = 48;

// 50 × 10 tiles (3200 × 640 px).
constexpr const char* kRows[] = {
    "..................................................",
    "..................................................",
    "..................................................",
    "..................................................",
    "..........GGGG..............GGGG..................",
    "................GGGG.............GGGG.............",
    ".....GGGG...........................GGGG..........",
    "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGFFFF",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
};

constexpr int kCols     = 50;
constexpr int kRowCount = 10;

bool isSolid(TileCell c)
{
    return c == TileCell::Grass || c == TileCell::Dirt || c == TileCell::Finish;
}

bool isWalkSurface(TileCell c)
{
    return c == TileCell::Grass || c == TileCell::Finish;
}

TileCell fromChar(char c)
{
    if (c == CGrass)
        return TileCell::Grass;
    if (c == CDirt)
        return TileCell::Dirt;
    if (c == CFinish)
        return TileCell::Finish;
    return TileCell::Empty;
}

void addMergedRowPhysics(b2WorldId world, int row, const TileMap& map)
{
    int segStart = -1;

    for (int col = 0; col <= map.cols; ++col) {
        const bool solid = col < map.cols && isSolid(map.at(col, row));

        if (solid && segStart < 0) {
            segStart = col;
            continue;
        }

        if (!solid && segStart >= 0) {
            const int   segEnd  = col - 1;
            const float width   = static_cast<float>(segEnd - segStart + 1) * TILE_SIZE;
            const float centerX = (static_cast<float>(segStart) + static_cast<float>(segEnd + 1))
                                  * 0.5f * static_cast<float>(TILE_SIZE);
            const float centerY = static_cast<float>(row) * TILE_SIZE + TILE_SIZE * 0.5f;
            createPhysicsPlatform(world, { centerX, centerY }, width, static_cast<float>(TILE_SIZE));
            segStart = -1;
        }
    }
}

int topGrassRowAtCol(const TileMap& map, int col)
{
    int best = -1;
    for (int row = 0; row < map.rows; ++row) {
        if (isWalkSurface(map.at(col, row)))
            best = row;
    }
    return best;
}

void spawnFinishGate(SDL_Texture* poleTex, SDL_Texture* signTex, int finishCol, int groundRow)
{
    if (!poleTex)
        return;

    const float surfaceY = static_cast<float>(groundRow) * static_cast<float>(TILE_SIZE);
    const float gateX    = static_cast<float>(finishCol) * TILE_SIZE + TILE_SIZE * 0.5f;

    constexpr float kPoleW = 12.f;
    constexpr float kPoleH = 120.f;
    const float     poleCy = surfaceY - kPoleH * 0.5f + 8.f;

    createDecoration(poleTex, { gateX - 56.f, poleCy }, kPoleW, kPoleH);
    createDecoration(poleTex, { gateX + 56.f, poleCy }, kPoleW, kPoleH);

    if (signTex)
        createDecoration(signTex, { gateX, surfaceY - 72.f }, 64.f, 64.f);
}

SDL_FPoint coinAboveTile(int col, int row)
{
    const float x = static_cast<float>(col) * TILE_SIZE + TILE_SIZE * 0.5f;
    const float y = static_cast<float>(row) * TILE_SIZE - 20.f;
    return { x, y };
}

void collectCoinSpawns(const TileMap& map, std::vector<SDL_FPoint>& out)
{
    out.clear();

    for (int row = 0; row < map.rows; ++row) {
        int segStart = -1;
        for (int col = 0; col <= map.cols; ++col) {
            const bool grass = col < map.cols && map.at(col, row) == TileCell::Grass;
            if (grass && segStart < 0) {
                segStart = col;
                continue;
            }
            if (!grass && segStart >= 0) {
                const int   midCol = (segStart + col - 1) / 2;
                const float topY   = static_cast<float>(row) * TILE_SIZE;
                if (row < kGroundRow && map.at(midCol, row - 1) == TileCell::Empty)
                    out.push_back({ static_cast<float>(midCol) * TILE_SIZE + TILE_SIZE * 0.5f,
                                    topY - 20.f });
                segStart = -1;
            }
        }
    }

    constexpr int kGroundCoins[] = { 8, 14, 20, 26, 32, 38, 42 };
    for (int col : kGroundCoins)
        out.push_back(coinAboveTile(col, kGroundRow));

    out.push_back(coinAboveTile(44, kGroundRow));
    out.push_back(coinAboveTile(45, kGroundRow));
}

} // namespace

void spawnCoins(b2WorldId world, SDL_Texture* coinTex,
                const std::vector<SDL_FPoint>& spawns, int value)
{
    if (!coinTex)
        return;
    for (const SDL_FPoint& pos : spawns)
        createCoin(world, coinTex, pos, value);
}

void destroyAllCoins()
{
    static const bagel::Mask mask = bagel::MaskBuilder()
        .set<SensorAreaComponent>()
        .set<CoinComponent>()
        .build();
    static int q = bagel::World::createQuery(mask);

    std::vector<bagel::Entity> pending;
    for (bagel::Entity e = bagel::World::first(q); !bagel::World::eof(q);
         e = bagel::World::next(q))
        pending.push_back(e);

    for (bagel::Entity e : pending) {
        const b2BodyId body = b2Shape_GetBody(e.get<SensorAreaComponent>().shape_id);
        b2DestroyBody(body);
        e.destroy();
    }
}

BuiltLevel buildTileLevel(b2WorldId world, TileMap& tileMap,
                          SDL_Texture* finishPoleTex, SDL_Texture* finishSignTex,
                          SDL_Texture* coinTex)
{
    BuiltLevel level;
    tileMap.cols = kCols;
    tileMap.rows = kRowCount;
    tileMap.cells.clear();
    tileMap.cells.reserve(static_cast<std::size_t>(kCols * kRowCount));

    for (int row = 0; row < kRowCount; ++row) {
        for (int col = 0; col < kCols; ++col)
            tileMap.cells.push_back(fromChar(kRows[row][col]));
    }

    level.widthPx  = static_cast<float>(kCols) * static_cast<float>(TILE_SIZE);
    level.heightPx = static_cast<float>(kRowCount) * static_cast<float>(TILE_SIZE);
    level.cameraY  = 0.f;

    for (int row = 0; row < tileMap.rows; ++row)
        addMergedRowPhysics(world, row, tileMap);

    constexpr int kStartCol = 3;
    const int     startRow  = topGrassRowAtCol(tileMap, kStartCol);
    const int     finishRow = topGrassRowAtCol(tileMap, kFinishCol);

    if (startRow >= 0) {
        const float surfaceY = static_cast<float>(startRow) * static_cast<float>(TILE_SIZE);
        level.playerStart    = { static_cast<float>(kStartCol) * TILE_SIZE + TILE_SIZE * 0.5f,
                                 surfaceY - 36.f };
    }

    if (finishRow >= 0) {
        const float surfaceY = static_cast<float>(finishRow) * static_cast<float>(TILE_SIZE);
        level.finishSensor   = { static_cast<float>(kFinishCol) * TILE_SIZE + TILE_SIZE * 0.5f,
                                 surfaceY + 48.f };
    }

    spawnFinishGate(finishPoleTex, finishSignTex, kFinishCol, kGroundRow);

    collectCoinSpawns(tileMap, level.coinSpawns);
    spawnCoins(world, coinTex, level.coinSpawns);

    return level;
}
