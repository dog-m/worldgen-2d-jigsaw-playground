#pragma once

#include <memory>
#include <deque>

#include "structures.hpp"
#include "world.hpp"
#include "dynamic_object_pool.hpp"

using StructurePlacementChecker = bool (*)(World const *world, int x, int y, StructureObject const *obj);

// ========================================================================

constexpr int COST_MAX = 80;

class StructureBuilder
{
private:
    World *world = nullptr;
    StructureProvider *structureProvider = nullptr;
    TileRegistry const *tileRegistry = nullptr;

    bool obstructed[WORLD_WIDTH * WORLD_HEIGHT] = {0};

    using JointCRef = Configuration::Structure::Joint const *;
    using JointSparseMap = std::unordered_map<Configuration::LocationHash, JointCRef>;
    JointSparseMap jointMap;

    inline void reg_joint_at(int const x, int const y, JointCRef const joint)
    {
        jointMap.emplace(Configuration::location_hash(x, y), joint);
    }

    inline JointCRef find_joint_at(int const x, int const y) const
    {
        if (auto const iter = jointMap.find(Configuration::location_hash(x, y)); iter != jointMap.cend())
            return iter->second;
        else
            return nullptr;
    }

    bool is_in_world(int const originX, int const originY, StructureObject const *const obj) const;
    bool is_free_space(int const originX, int const originY, StructureObject const *const obj) const;
    bool is_compatible(int const originX, int const originY, StructureObject const *const obj) const;
    bool is_satisfied(int const originX, int const originY, StructureObject const *const obj) const;
    bool is_continuous(int const originX, int const originY, StructureObject const *const obj) const;
    
    void claim_space(int const originX, int const originY, StructureObject const *const obj);

    struct BuildRequest
    {
        int x;
        int y;
        StructureObject const *obj;
        int totalCost;
    };

    misc::ObjectPoolDynamic<BuildRequest> requestPool;
    std::deque<BuildRequest *> buildQueue;

    void build(BuildRequest const *const request);
    void propagate(BuildRequest const *const request);
    void propagate_joint(BuildRequest const *const request, JointCRef const joint);

    std::unordered_map<std::string, StructurePlacementChecker> placementCheckers;

public:
    explicit StructureBuilder();

    void attach_world(World *const worldPtr);

    void attach_structure_provider(StructureProvider *const provider);

    void attach_tile_registry(TileRegistry const *registry);

    void reset();

    bool request_structure_at(std::string const &structureId,
                              int expectedJointWorldX,
                              int expectedJointWorldY,
                              int targetJointDirX,
                              int targetJointDirY,
                              std::string const &targetTag,
                              int cost);

    void process_all_requests();
};

// ========================================================================

class WorldGenerator
{
private:
    StructureProvider provider;
    StructureBuilder builder;

    void gen_soil(World *const world);
    void gen_base(World *const world);

public:
    void generate(World *const world, TileRegistry const *const tileRegistry);
};
