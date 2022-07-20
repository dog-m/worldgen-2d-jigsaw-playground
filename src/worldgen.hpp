#pragma once

#include <memory>
#include <deque>

#include "structures.hpp"
#include "world.hpp"
#include "dynamic_object_pool.hpp"

using StructurePlacementChecker = bool (*)(World const *world, int x, int y, StructureObject const *obj);

// ========================================================================

class StructureBuilder
{
private:
    World *world = nullptr;
    StructureProvider *structureProvider = nullptr;
    TileRegistry const *tileRegistry = nullptr;

    bool obstructed[WORLD_WIDTH * WORLD_HEIGHT] = {0};
    uint16_t reserved[WORLD_WIDTH * WORLD_HEIGHT] = {0};

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

    bool is_in_world(int originX, int originY, StructureObject const *obj) const;
    bool is_free_space(int originX, int originY, StructureObject const *obj) const;
    bool is_unreserved(int originX, int originY, StructureObject const *obj, uint16_t id) const;
    bool is_compatible(int originX, int originY, StructureObject const *obj) const;
    bool is_satisfied(int originX, int originY, StructureObject const *obj) const;
    bool is_continuable(int originX, int originY, StructureObject const *obj, uint16_t requestId);

    void reserve_space(int originX, int originY, StructureObject const *obj, uint16_t id);
    void claim_space(int originX, int originY, StructureObject const *obj);
    void place_joints(int originX, int originY, StructureObject const *obj);

    struct BuildRequest
    {
        int x;
        int y;
        StructureObject const *obj;
        int budget;
        uint16_t idSrc;
        uint16_t id;
    };
    uint16_t uidGenerator = 0;

    inline uint16_t getUID()
    {
        ++uidGenerator;
        if (uidGenerator == 0)
            ++uidGenerator;

        return uidGenerator;
    }

    misc::ObjectPoolDynamic<BuildRequest, 32> requestPool;
    std::deque<BuildRequest *> buildQueue;

    void build(BuildRequest const *const request);
    void propagate(BuildRequest const *const request);

    BuildRequest *propagate_joint(int originX,
                                  int originY,
                                  StructureObject const *obj,
                                  JointCRef const joint,
                                  int budget,
                                  uint16_t requestId,
                                  uint16_t requestSrcId,
                                  bool checkContinuity);

    BuildRequest *try_request_structure(std::string const &structureId,
                                        int expectedJointWorldX,
                                        int expectedJointWorldY,
                                        int targetJointDirX,
                                        int targetJointDirY,
                                        std::string const &targetTag,
                                        int budget,
                                        uint16_t requestId,
                                        uint16_t requestSrcId,
                                        bool checkContinuity);

    std::unordered_map<std::string, StructurePlacementChecker> placementCheckers;

public:
    explicit StructureBuilder();

    void attach_world(World *const worldPtr);

    void attach_structure_provider(StructureProvider *const provider);

    void attach_tile_registry(TileRegistry const *registry);

    void reset();

    void request_structure(std::string const &structureId,
                           int jointWorldX,
                           int jointWorldY,
                           std::string const &jointTag,
                           int budget);

    void process_all_requests();

    bool step();
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

    bool step();
};
