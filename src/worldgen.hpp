#pragma once

#include <memory>
#include <deque>

#include "structures.hpp"
#include "world.hpp"

using StructurePlacementChecker = bool (*)(World const *world, int x, int y, StructureObject const *obj);

// ========================================================================

constexpr int COST_MAX = 8;

class StructureBuilder
{
private:
    World *world = nullptr;
    StructureProvider *structureProvider = nullptr;
    TileRegistry const *tileRegistry = nullptr;

    bool obstructed[WORLD_WIDTH * WORLD_HEIGHT] = {0};

    void claimStructureSpace(int const callerX, int const callerY, StructureObject const *const obj);
    bool can_be_built(int const callerX, int const callerY, StructureObject const *const obj) const;

    struct BuildRequest
    {
        int x;
        int y;
        StructureObject const *obj;
        int cost;
    };

    std::deque<std::unique_ptr<BuildRequest>> buildQueue;

    void build(int const callerX, int const callerY, StructureObject const *const obj, int const cost);
    void propagate(int const callerX, int const callerY, StructureObject const *const obj, int const cost);

    std::unordered_map<std::string, StructurePlacementChecker> placementCheckers;

public:
    explicit StructureBuilder();

    void attachWorld(World *const worldPtr);

    void attachStructureProvider(StructureProvider *const provider);

    void attachTileRegistry(TileRegistry const *registry);

    void reset();

    bool requestStructureAt(int x, int y, std::string const &structureId, std::string const &targetJoint, int cost);

    void processAllRequests();
};

// ========================================================================

class WorldGenerator
{
private:
    StructureProvider provider;
    StructureBuilder builder;

    static inline float fractalNoise(int octaves, float x, float y = 0.f, float z = 0.f);

    void genSoil(World *const world);
    void genBase(World *const world);

public:
    void generate(World *const world, TileRegistry const *const tileRegistry);
};
