#include "LevelBuilder.h"
#include "Components.h"
#include "EntityFactory.h"
#include "Game.h"
#include "TileConfig.h"
#include <bagel.h>
#include <unordered_map>
#include <vector>

namespace {

enum CharCell : char {
    CEmpty         = '.',
    CGrass         = 'G',
    CDirt          = 'D',
    CFinish        = 'F',
    CCloud         = 'C',
    CCoin          = 'O',
    CQuestionBlock = 'Q',
};

constexpr int kGroundRow  = 14;
constexpr int kFinishCol  = 96;
constexpr int kStartCol   = 3;

// 100 × 20 tiles  (6400 × 1280 px)
constexpr const char* kRows[] = {
    "....................................................................................................",
    "....................................................................................................",
    "....................................................................................................",
    ".............CCC.................................................................................CC.",
    "..........................CCCC........CCCCCC..........CCC...........................................",
    "...CCC...........................................O.......................Q....O.....................",
    "............................................................................CCCCCC.......CCC........",
    "...............O...............................CCCCC................................................",
    ".................................Q..............................GGGGGGG.............................",
    ".........Q..........GGGGG..............GGGGGG.......................................................",
    "...............................DDDD.................................................................",
    ".......GGGGG...................DDDDD....................GGGGGG............GGGGGGGGG.................",
    "......................Q.......DDDDDD.............Q..................................................",
    ".............................DDDDDDD................................................................",
    "GGGGGGGGGGGGGG...GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG.....GGGGGGGGGGGGGGGGGGGGGGGGGGFFFF",
    "DDDDDDDDDDDDDD...DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD..O..DDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
};

constexpr int kCols     = 100;
constexpr int kRowCount = 20;

bool isSolid(TileCell c)
{
    return c == TileCell::Grass || c == TileCell::Dirt
        || c == TileCell::Finish || c == TileCell::Cloud;
    // QuestionBlock is NOT merged — each gets its own body so it can be destroyed individually.
}

bool isWalkSurface(TileCell c)
{
    return c == TileCell::Grass || c == TileCell::Finish
        || c == TileCell::QuestionBlock;
}

TileCell fromChar(char c)
{
    if (c == CGrass)         return TileCell::Grass;
    if (c == CDirt)          return TileCell::Dirt;
    if (c == CFinish)        return TileCell::Finish;
    if (c == CCloud)         return TileCell::Cloud;
    if (c == CCoin)          return TileCell::Coin;
    if (c == CQuestionBlock) return TileCell::QuestionBlock;
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

void addQBlockPhysics(b2WorldId world, const TileMap& map,
                      std::unordered_map<uint32_t, bagel::Entity>& out)
{
    for (int row = 0; row < map.rows; ++row) {
        for (int col = 0; col < map.cols; ++col) {
            if (map.at(col, row) != TileCell::QuestionBlock)
                continue;

            const float cx = (static_cast<float>(col) + 0.5f) * TILE_SIZE;
            const float cy = (static_cast<float>(row) + 0.5f) * TILE_SIZE;

            // Solid body but filtered to never collide with the player (PHYS_CAT_DEFAULT).
            // The player's shape mask excludes PHYS_CAT_QBLOCK, so no physical interaction
            // occurs from any direction. Head-hit detection in qblock_system is position-based.
            b2BodyDef bodyDef  = b2DefaultBodyDef();
            bodyDef.type       = b2_staticBody;
            bodyDef.position.x = cx / BOX_SCALE;
            bodyDef.position.y = cy / BOX_SCALE;
            b2BodyId body      = b2CreateBody(world, &bodyDef);

            b2ShapeDef shapeDef          = b2DefaultShapeDef();
            shapeDef.filter.categoryBits = PHYS_CAT_QBLOCK;
            shapeDef.filter.maskBits     = 0; // collides with nothing
            const float hw  = static_cast<float>(TILE_SIZE) * 0.5f / BOX_SCALE;
            b2Polygon   box = b2MakeBox(hw, hw);
            b2ShapeId   shape = b2CreatePolygonShape(body, &shapeDef, &box);

            bagel::Entity e = bagel::Entity::create();
            e.addAll(
                TransformComponent  { { cx, cy }, 0.f },
                PhysicsBodyComponent{ body, shape, false, 0, 0 }
            );

            const uint32_t key = static_cast<uint32_t>(col)
                               | (static_cast<uint32_t>(row) << 16);
            out.emplace(key, e);
        }
    }
}

void collectCoinSpawns(const TileMap& map, std::vector<SDL_FPoint>& out)
{
    out.clear();

    // Coins above the midpoint of every floating platform (rows above ground)
    for (int row = 0; row < kGroundRow; ++row) {
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
                if (row > 0 && map.at(midCol, row - 1) == TileCell::Empty)
                    out.push_back({ static_cast<float>(midCol) * TILE_SIZE + TILE_SIZE * 0.5f,
                                    topY - 20.f });
                segStart = -1;
            }
        }
    }

    // Explicit coins on the main ground segments
    constexpr int kGroundCoins[] = { 7, 26, 40, 56, 71, 88 };
    for (int col : kGroundCoins)
        out.push_back(coinAboveTile(col, kGroundRow));

    // Coins placed with 'O' tiles
    for (int row = 0; row < map.rows; ++row) {
        for (int col = 0; col < map.cols; ++col) {
            if (map.at(col, row) == TileCell::Coin)
                out.push_back(coinAboveTile(col, row));
        }
    }
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

    // Invisible ceiling across the entire level to allow running on the top of the screen
    createPhysicsPlatform(world, { level.widthPx * 0.5f, -10.f }, level.widthPx, 20.f);

    for (int row = 0; row < tileMap.rows; ++row)
        addMergedRowPhysics(world, row, tileMap);

    addQBlockPhysics(world, tileMap, level.qBlockBodies);

    const int startRow  = topGrassRowAtCol(tileMap, kStartCol);
    const int finishRow = topGrassRowAtCol(tileMap, kFinishCol);

    if (startRow >= 0) {
        const float surfaceY = static_cast<float>(startRow) * static_cast<float>(TILE_SIZE);
        // offset = body HH (36) + 12px gap above ground
        level.playerStart = { static_cast<float>(kStartCol) * TILE_SIZE + TILE_SIZE * 0.5f,
                              surfaceY - 48.f };
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
