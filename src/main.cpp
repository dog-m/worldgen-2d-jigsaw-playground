#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <string>
#include <iostream>
#include <fstream>

#include <raylib.h>
#include <nlohmann/json.hpp>

extern "C"
{
#include "thirdparty/noise/noise1234.h"
}

// ========================================================================

namespace Configuration
{
    struct Structure
    {
        struct Target
        {
            std::string structureId;
            std::string joint;
            int32_t weight;
        };

        struct Joint
        {
            std::array<uint16_t, 2> location;
            std::array<int16_t, 2> direction;
            std::string replaceBy;
            std::vector<Target> structures;
        };

        int32_t cost;
        std::unordered_map<std::string, Joint> joints;
        std::unordered_set<std::string> placementConstraints;
        std::unordered_map<uint32_t, std::string> colorsToBlocks;

        std::unordered_map<uint16_t, std::string> coordToJoint;
    };

    void from_json(const nlohmann::json &j, Structure::Target &t)
    {
        j.at("id").get_to(t.structureId);
        j.at("joint").get_to(t.joint);
        j.at("weight").get_to(t.weight);
    }

    void from_json(const nlohmann::json &j, Structure::Joint &sj)
    {
        j.at("location").get_to(sj.location);
        j.at("direction").get_to(sj.direction);
        j.at("replace-by").get_to(sj.replaceBy);
        j.at("structures").get_to(sj.structures);
    }

    void from_json(const nlohmann::json &j, Structure &s)
    {
        j.at("cost").get_to(s.cost);
        j.at("joints").get_to(s.joints);
        j.at("placement-constraints").get_to(s.placementConstraints);

        auto const x = j.at("color-to-tile-mapping")
                           .get<std::unordered_map<std::string, std::array<uint32_t, 3>>>();
        for (auto [name, rgb] : x)
        {
            auto [r, g, b] = rgb;
            auto const color = (r << 16) + (g << 8) + b;
            s.colorsToBlocks.emplace(color, name);
        }

        // post-processing
        for (auto const [name, joint] : s.joints)
        {
            auto const coords = (joint.location[0] << 8) + joint.location[1];
            s.coordToJoint.emplace(coords, name);
        }
    }
}

// ========================================================================

using TileId = uint8_t;

constexpr TileId AIR = 0;
constexpr TileId SOIL = 1;
constexpr TileId STONE = 2;

constexpr TileId BACKGROUND = 3;
constexpr TileId WALL = 4;
constexpr TileId CHAIN = 5;

constexpr TileId STRUCTURE_VOID = 128;
constexpr TileId STRUCTURE_JOINT = 129;

constexpr TileId UNKNOWN = 255;

class TileRegistry
{
private:
    std::unordered_map<TileId, Color> colors;
    std::unordered_map<std::string, TileId> names;

public:
    TileId getTile(std::string const &name) const
    {
        if (auto const iter = names.find(name); iter != names.cend())
            return iter->second;
        else
            return UNKNOWN;
    }

    Color getTileColor(TileId tile) const
    {
        if (auto const iter = colors.find(tile); iter != colors.cend())
            return iter->second;
        else
            return RED;
    }

    void registerTile(TileId const tile, std::string const &name, Color color)
    {
        color.a = 255;
        names[name] = tile;
        colors[tile] = color;
    }
};

// ========================================================================

constexpr int WORLD_WIDTH = 1000;
constexpr int WORLD_WIDTH_M1 = WORLD_WIDTH - 1;

constexpr int WORLD_HEIGHT = 350;
constexpr int WORLD_HEIGHT_M1 = WORLD_HEIGHT - 1;

class World
{
private:
    TileId tiles[WORLD_WIDTH * WORLD_HEIGHT] = {0};
    uint16_t heightMap[WORLD_WIDTH] = {0};

public:
    void setTile(int x, int y, TileId tile)
    {
        if (x < 0 || x > WORLD_WIDTH_M1 ||
            y < 0 || y > WORLD_HEIGHT_M1)
            return;

        tiles[x + y * WORLD_WIDTH] = tile;
        auto &height = heightMap[x];

        if (tile == AIR)
        {
            if (y == height)
                while (height > 0 && tiles[x + height * WORLD_WIDTH] == AIR)
                    --height;
        }
        else
        {
            if (height < y)
                height = y;
        }
    }

    TileId getTileAt(int x, int y) const
    {
        if (x < 0 || x > WORLD_WIDTH_M1 ||
            y < 0 || y > WORLD_HEIGHT_M1)
            return AIR;

        return tiles[x + y * WORLD_WIDTH];
    }

    int getHeightAt(int const x) const
    {
        if (x < 0 || x > WORLD_WIDTH_M1)
            return 0;
        else
            return heightMap[x];
    }

    void clear()
    {
        std::fill(std::begin(tiles), std::end(tiles), AIR);
        std::fill(std::begin(heightMap), std::end(heightMap), 0);
    }

    void render(Image *const img, TileRegistry const *const registry) const
    {
        auto tilePtr = tiles + WORLD_HEIGHT_M1 * WORLD_WIDTH;
        auto pixel = (Color *)img->data;

        for (int y = 0; y < WORLD_HEIGHT; y++, tilePtr -= WORLD_WIDTH * 2)
            for (int x = 0; x < WORLD_WIDTH; x++, tilePtr++, pixel++)
                *pixel = registry->getTileColor(*tilePtr);
    }
};

// ========================================================================

struct StructureObject
{
    int width;
    int height;

    Configuration::Structure config;

    std::vector<TileId> tiles;
};

class StructureProvider
{
private:
    std::unordered_map<std::string, std::unique_ptr<StructureObject>> loadedStructures;
    TileRegistry const *tileRegistry = nullptr;

    StructureObject const *loadStructure(std::string const &id)
    {
        auto p = loadedStructures.emplace(id, std::make_unique<StructureObject>());
        auto result = p.first->second.get();

        // load the configuration
        std::ifstream in("../../res/" + id + ".json");
        result->config = nlohmann::json::parse(in).get<Configuration::Structure>();

        // load image
        auto img = LoadImage(("../../res/" + id + ".png").c_str());
        result->width = img.width;
        result->height = img.height;

        // change orientation before-hand for ease of use
        ImageFlipVertical(&img);
        ImageFormat(&img, PixelFormat::UNCOMPRESSED_R8G8B8A8);
        // convert to tiles

        result->tiles = std::vector<TileId>(img.width * img.height);

        auto pixel = (Color const *)img.data;
        auto tile = result->tiles.data();
        auto const tileLast = tile + img.width * img.height;
        for (; tile != tileLast; pixel++, tile++)
        {
            auto const color = (pixel->r << 16) + (pixel->g << 8) + pixel->b;
            auto const name = result->config.colorsToBlocks[color];
            *tile = tileRegistry->getTile(name);
        }

        UnloadImage(img);

        return result;
    }

public:
    void attachTileRegistry(TileRegistry const *const registry)
    {
        this->tileRegistry = registry;
    }

    StructureObject const *getStructure(std::string const &id)
    {
        if (auto const iter = loadedStructures.find(id); iter != loadedStructures.end())
            return iter->second.get();
        else
            return loadStructure(id);
    }
};

// ========================================================================

using StructurePlacementChecker = bool (*)(World const *world, int x, int y, StructureObject const *obj);

static bool undergroundPlacementChecker(
    World const *world,
    int x,
    int y,
    StructureObject const *obj)
{
    y += obj->height - 1;

    for (int dx = 0; dx < obj->width; dx++)
        if (y > world->getHeightAt(x + dx))
            return false;
    return true;
}

static bool noBlocksPlacementChecker(
    World const *world,
    int x,
    int y,
    StructureObject const *obj)
{
    for (int dy = 0; dy < obj->height; dy++)
        for (int dx = 0; dx < obj->width; dx++)
            if (world->getTileAt(x + dx, y + dy) != AIR)
                return false;
    return true;
}

// ========================================================================

constexpr int COST_MAX = 8;

class StructureBuilder
{
private:
    World *world = nullptr;
    StructureProvider *structureProvider = nullptr;
    TileRegistry const *tileRegistry = nullptr;

    bool obstructed[WORLD_WIDTH * WORLD_HEIGHT] = {0};

    void claimStructureSpace(
        int const callerX,
        int const callerY,
        StructureObject const *const obj)
    {
        auto tilePtr = obj->tiles.data();
        auto obstruction = obstructed + callerX + callerY * WORLD_WIDTH;

        for (int y = 0; y < obj->height; y++, obstruction += WORLD_WIDTH - obj->width)
            for (int x = 0; x < obj->width; x++, tilePtr++, obstruction++)
                // is it an actual structure part?
                if (*tilePtr != STRUCTURE_VOID)
                    *obstruction = true;
    }

    bool can_be_build(
        int const callerX,
        int const callerY,
        StructureObject const *const obj) const
    {
        if (callerX < 0 || callerX + obj->width > WORLD_WIDTH_M1 ||
            callerY < 0 || callerY + obj->height > WORLD_HEIGHT_M1)
            return false;

        auto tilePtr = obj->tiles.data();
        auto obstruction = obstructed + callerX + callerY * WORLD_WIDTH;

        for (int y = 0; y < obj->height; y++, obstruction += WORLD_WIDTH - obj->width)
            for (int x = 0; x < obj->width; x++, tilePtr++, obstruction++)
                // is this part already occupied by some other structure?
                if (*tilePtr != STRUCTURE_VOID && *obstruction)
                    return false;

        return true;
    }

    struct BuildRequest
    {
        int x;
        int y;
        StructureObject const *obj;
        int cost;
    };

    std::deque<std::unique_ptr<BuildRequest>> buildQueue;

    void build(
        int const callerX,
        int const callerY,
        StructureObject const *const obj,
        int const cost)
    {
        auto tilePtr = obj->tiles.data();

        // materialize the structure
        for (int y = 0; y < obj->height; y++)
        {
            for (int x = 0; x < obj->width; x++, tilePtr++)
                // is it an actual structure part?
                switch (*tilePtr)
                {
                case STRUCTURE_VOID:
                    // just ignore it
                    break;

                case STRUCTURE_JOINT:
                {
                    // place replacement tile instead
                    auto const cIndex = (x << 8) + (obj->height - 1 - y);
                    auto const &jointName = obj->config.coordToJoint.at(cIndex);
                    auto const &joint = obj->config.joints.at(jointName);

                    world->setTile(callerX + x, callerY + y, tileRegistry->getTile(joint.replaceBy));
                    break;
                }

                default:
                    world->setTile(callerX + x, callerY + y, *tilePtr);
                    break;
                }
        }
    }

    void propagate(
        int const callerX,
        int const callerY,
        StructureObject const *const obj,
        int const cost)
    {
        std::vector<Configuration::Structure::Target const *> targets;

        for (auto const &[_, joint] : obj->config.joints)
            if (joint.structures.size() > 0)
            {
                // queue the following structure (structure-specific alignment will be done separately)
                auto const newX = callerX + joint.direction[0] + joint.location[0];
                auto const newY = callerY + joint.direction[1] + (obj->height - 1 - joint.location[1]);

                // prepare the pool of target structures
                targets.clear();
                uint32_t totalWeight = 0;
                for (auto const &target : joint.structures)
                {
                    targets.emplace_back(&target);
                    totalWeight += target.weight;
                }

                // enqueue the random placeable target
                while (!targets.empty())
                {
                    Configuration::Structure::Target const *target = nullptr;

                    if (totalWeight == 0 || targets.size() == 1)
                    {
                        // pick the only thing left
                        target = *targets.begin();
                        targets.clear();
                    }
                    else
                    {
                        // pick a random value
                        auto const value = rand() % totalWeight;

                        // find anything that is above the threshold
                        auto weightSum = 0;
                        for (auto it = targets.cbegin(); it != targets.cend(); it++)
                        {
                            auto const candidate = *it;
                            weightSum += candidate->weight;

                            if (weightSum > value)
                            {
                                target = candidate;

                                // remove the selected thing from the pool unrelated to placement successfulness
                                targets.erase(it);
                                totalWeight -= target->weight;

                                break;
                            }
                        }
                    }

                    // attempt to place the thing
                    if (requestStructureAt(newX, newY, target->structureId, target->joint, cost))
                        break;
                }
            }
    }

    std::unordered_map<std::string, StructurePlacementChecker> placementCheckers;

public:
    StructureBuilder()
    {
        this->placementCheckers.emplace("underground", undergroundPlacementChecker);
        this->placementCheckers.emplace("no-blocks", noBlocksPlacementChecker);
    }

    void attachWorld(World *const worldPtr)
    {
        this->world = worldPtr;
    }

    void attachStructureProvider(StructureProvider *const provider)
    {
        this->structureProvider = provider;
    }

    void attachTileRegistry(TileRegistry const *registry)
    {
        this->tileRegistry = registry;
    }

    void reset()
    {
        std::fill(std::begin(obstructed), std::end(obstructed), false);
    }

    bool requestStructureAt(
        int x,
        int y,
        std::string const &structureId,
        std::string const &targetJoint,
        int cost)
    {
        // find the structure and correct the origin point
        auto const obj = structureProvider->getStructure(structureId);
        auto const &joint = obj->config.joints.at(targetJoint);
        x -= joint.location[0];
        y -= obj->height - 1 - joint.location[1];

        // correct the cost of current building branch
        cost += obj->config.cost;
        if (cost > COST_MAX)
            return false;

        // see if there is enougth space
        if (!can_be_build(x, y, obj))
            return false;

        // check placement constraints
        for (auto const &constraint : obj->config.placementConstraints)
            if (auto const &checker = placementCheckers.at(constraint); !checker(world, x, y, obj))
                return false;

        // queue and claim space for it
        auto req = std::make_unique<BuildRequest>();
        req->x = x;
        req->y = y;
        req->obj = obj;
        req->cost = cost;
        buildQueue.emplace_back(std::move(req));
        claimStructureSpace(x, y, obj);

        return true;
    }

    void processAllRequests()
    {
        while (!buildQueue.empty())
        {
            auto request = std::move(buildQueue.front());
            buildQueue.pop_front();

            // materialize the thing and propagate ongoing structures further after its joints
            build(request->x, request->y, request->obj, request->cost);
            propagate(request->x, request->y, request->obj, request->cost);
        }
    }
};

// ========================================================================

class WorldGenerator
{
private:
    float fractalNoise(int octaves, float x, float y = 0, float z = 0)
    {
        auto result = 0.f;
        auto scale = 1.0f;
        auto k = 0.5f;

        for (int i = 0; i < octaves; i++)
        {
            scale *= 0.5f;
            k *= 2.f;

            result += scale * (noise3(x * k, y * k, z * k) + 1.0f) * 0.5f;
        }

        return result;
    }

    void genSoil(World *const world)
    {
        auto const z = 1; // rand() % 256;

        for (int y = 0; y < WORLD_HEIGHT; y++)
            for (int x = 0; x < WORLD_WIDTH; x++)
            {
                auto const n1 = fractalNoise(3, x / 128.f, z);
                auto const n2 = fractalNoise(3, x / 64.f, y / 64.f, z) * (WORLD_HEIGHT - y) / WORLD_HEIGHT;
                auto const n3 = fractalNoise(2, x / 32.f, y / 16.f, z + 1.f);

                if (n3 > 0.385 * 0.85)
                {
                    if (y < n1 * WORLD_HEIGHT)
                        world->setTile(x, y, SOIL);

                    if (n2 > 0.3f)
                        world->setTile(x, y, STONE);
                }
                /*if (y < (WORLD_HEIGHT >> 1))
                    world->setTile(x, y, STONE);*/
            }
    }

    StructureProvider provider;
    StructureBuilder builder;

    void genBase(World *const world)
    {
        int const startX = 15 + rand() % (WORLD_WIDTH_M1 - 15 * 2);
        int const startY = world->getHeightAt(startX) - 2;

        builder.requestStructureAt(startX, startY, "room/base", "#floor", 0);
        builder.processAllRequests();
    }

public:
    void generate(World *const world, TileRegistry const *const tileRegistry)
    {
        provider.attachTileRegistry(tileRegistry);

        builder.reset();
        builder.attachTileRegistry(tileRegistry);
        builder.attachStructureProvider(&provider);
        builder.attachWorld(world);

        genSoil(world);
        genBase(world);
    }
};

// ========================================================================

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 1000;
    const int screenHeight = 350;

    InitWindow(screenWidth, screenHeight, "2D Worldgen Testing Ground");
    SetTargetFPS(20);

    auto const tiles = std::make_unique<TileRegistry>();
    tiles->registerTile(AIR, "blocks/air", BLACK);
    tiles->registerTile(SOIL, "blocks/soil", DARKPURPLE);
    tiles->registerTile(STONE, "blocks/stone", DARKBLUE);
    tiles->registerTile(BACKGROUND, "blocks/background", DARKGRAY);
    tiles->registerTile(WALL, "blocks/wall", LIGHTGRAY);
    tiles->registerTile(CHAIN, "blocks/chain", LIME);

    tiles->registerTile(STRUCTURE_VOID, "structure/void", RED);
    tiles->registerTile(STRUCTURE_JOINT, "structure/joint", RED);

    auto const world = std::make_unique<World>();
    auto const gen = std::make_unique<WorldGenerator>();

    auto imgWorld = GenImageColor(WORLD_WIDTH, WORLD_HEIGHT, BLACK);
    auto texWorld = LoadTextureFromImage(imgWorld);
    Vector2 posWorld = {0};

    Vector2 ballPosition = {-100.0f, -100.0f};
    auto scale = 1.f;

    // Main loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        ballPosition = GetMousePosition();
        scale += 0.5f * GetMouseWheelMove();

        IsMouseButtonDown(MouseButton::MOUSE_LEFT_BUTTON);

        if (IsKeyPressed(KeyboardKey::KEY_SPACE))
        {
            world->clear();

            gen->generate(world.get(), tiles.get());
            world->render(&imgWorld, tiles.get());

            UpdateTexture(texWorld, imgWorld.data);
        }

        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(BLACK);

        DrawTextureEx(texWorld, posWorld, 0.f, scale, WHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    UnloadTexture(texWorld);
    UnloadImage(imgWorld);
    CloseWindow();

    return 0;
}
