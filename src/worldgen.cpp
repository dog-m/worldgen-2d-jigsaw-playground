#include "worldgen.hpp"

extern "C"
{
#include "thirdparty/noise/noise1234.h"
}

namespace PlacementCheckers
{

    static bool underground(
        World const *world,
        int x,
        int y,
        StructureObject const *obj)
    {
        y += obj->height - 1;

        for (int dx = 0; dx < obj->width; dx++)
            if (y > world->get_height_at(x + dx))
                return false;

        return true;
    }

    static bool no_tiles(
        World const *world,
        int x,
        int y,
        StructureObject const *obj)
    {
        for (int dy = 0; dy < obj->height; dy++)
            for (int dx = 0; dx < obj->width; dx++)
                if (world->get_tile_at(x + dx, y + dy) != Tiles::AIR)
                    return false;

        return true;
    }

}

// ========================================================================

void StructureBuilder::claim_space(
    int const originX,
    int const originY,
    StructureObject const *const obj,
    bool *const dimension)
{
    auto tilePtr = obj->tiles.data();
    auto obstruction = dimension + originX + originY * WORLD_WIDTH;

    for (int y = 0; y < obj->height; y++, obstruction += WORLD_WIDTH - obj->width)
        for (int x = 0; x < obj->width; x++, tilePtr++, obstruction++)
            // is it an actual structure part?
            if (*tilePtr != Tiles::STRUCTURE_VOID)
                *obstruction = true;
}

void StructureBuilder::place_joints(
    int const originX,
    int const originY,
    StructureObject const *obj)
{
    for (auto const &[_, joint] : obj->config.joints)
    {
        auto const jx = originX + joint.location[0];
        auto const jy = originY + obj->height - 1 - joint.location[1];
        reg_joint_at(jx, jy, &joint);
    }
}

/// Can this structure exists at all?
bool StructureBuilder::is_in_world(
    int originX,
    int originY,
    StructureObject const *obj) const
{
    if (originX < 0 || originX + obj->width > WORLD_WIDTH_M1 ||
        originY < 0 || originY + obj->height > WORLD_HEIGHT_M1)
        return false;

    return true;
}

// Is there enougth unoccupied space for the structure?
bool StructureBuilder::is_free_space(
    int originX,
    int originY,
    StructureObject const *obj,
    bool const *dimension) const
{
    auto tilePtr = obj->tiles.data();
    auto obstruction = dimension + originX + originY * WORLD_WIDTH;

    for (int y = 0; y < obj->height; y++, obstruction += WORLD_WIDTH - obj->width)
        for (int x = 0; x < obj->width; x++, tilePtr++, obstruction++)
            // is this part already occupied by some other structure?
            if (*tilePtr != Tiles::STRUCTURE_VOID && *obstruction)
                return false;

    return true;
}

/// Are the joints of this structure compatible with already placed joints?
bool StructureBuilder::is_compatible(
    int originX,
    int originY,
    StructureObject const *obj) const
{
    for (auto const &[_, joint] : obj->config.joints)
    {
        auto const jx = originX + joint.direction[0] + joint.location[0];
        auto const jy = originY + joint.direction[1] + obj->height - 1 - joint.location[1];

        auto const placedJoint = find_joint_at(jx, jy);
        if (placedJoint)
        {
            if (!placedJoint->isFacing(joint) || !placedJoint->isCompatibleWith(joint))
                return false;
        }
        else
        {
            if (obstructed[jx + jy * WORLD_WIDTH])
                return false;
        }
    }

    return true;
}

// Are all the placement constraints sattisfied or not?
bool StructureBuilder::is_satisfied(
    int originX,
    int originY,
    StructureObject const *obj) const
{
    for (auto const &constraint : obj->config.placementConstraints)
        if (auto const &checker = placementCheckers.at(constraint); !checker(world, originX, originY, obj))
            return false;

    return true;
}

// Can the suggested structure be continued after being placed?
bool StructureBuilder::is_continuable(
    int originX,
    int originY,
    StructureObject const *obj)
{
    std::vector<StructureBuilder::BuildRequest *> supposedStructures;

    for (auto const &[_, joint] : obj->config.joints)
    {
        auto const jx = originX + joint.direction[0] + joint.location[0];
        auto const jy = originY + joint.direction[1] + obj->height - 1 - joint.location[1];
        // ignore "already connected" directions
        if (find_joint_at(jx, jy) || joint.direction[0] == 0 && joint.direction[1] == 0)
            continue;

        // on every disconnected joint it should be possible to attach at least something
        auto const newReq = propagate_joint(originX, originY, obj, &joint, 1000, false);
        if (!newReq)
        {
            // clean-up the objects
            for (auto const req : supposedStructures)
                requestPool.release(req);

            return false;
        }

        supposedStructures.emplace_back(newReq);
    }

    // reserve the space and free the objects
    for (auto const req : supposedStructures)
    {
        claim_space(req->x, req->y, req->obj, reserved);

        requestPool.release(req);
    }

    return true;
}

void StructureBuilder::build(BuildRequest const *const request)
{
    auto tilePtr = request->obj->tiles.data();

    // materialize the structure
    for (int y = 0; y < request->obj->height; y++)
        for (int x = 0; x < request->obj->width; x++, tilePtr++)
            // is it an actual structure part?
            switch (*tilePtr)
            {
            case Tiles::STRUCTURE_VOID:
                // just ignore it
                break;

            case Tiles::STRUCTURE_JOINT:
            {
                // place replacement tile instead
                auto const cIndex = Configuration::location_hash(x, request->obj->height - 1 - y);
                auto const &joint = request->obj->config.joints.at(cIndex);

                world->set_tile(request->x + x, request->y + y, tileRegistry->get_tile(joint.replaceBy));
                break;
            }

            default:
                world->set_tile(request->x + x, request->y + y, *tilePtr);
                break;
            }
}

void StructureBuilder::propagate(BuildRequest const *const request)
{
    for (auto const [_, joint] : request->obj->config.joints)
    {
        auto const newReq = propagate_joint(request->x, request->y, request->obj, &joint, request->budget, true);
        if (!newReq)
            continue;

        // schedule the placement
        buildQueue.emplace_back(newReq);

        // claim space and pre-register joints
        claim_space(newReq->x, newReq->y, newReq->obj, obstructed);
        place_joints(newReq->x, newReq->y, newReq->obj);
    }
}

StructureBuilder::BuildRequest *StructureBuilder::propagate_joint(
    int originX,
    int originY,
    StructureObject const *obj,
    JointCRef const joint,
    int budget,
    bool checkContinuity)
{
    auto const targetJointX = originX + joint->direction[0] + joint->location[0];
    auto const targetJointY = originY + joint->direction[1] + obj->height - 1 - joint->location[1];
    // skip directions with "already connected" joints
    if (find_joint_at(targetJointX, targetJointY))
        return nullptr;

    std::unordered_set<Configuration::Pool::Entry const *> targets;

    auto pool = structureProvider->get_pool(joint->structurePool);
    while (pool && !pool->structures.empty())
    {
        // prepare the choise table
        auto totalWeight = 0;
        for (auto const &entry : pool->structures)
        {
            targets.insert(std::addressof(entry));
            totalWeight += entry.weight;
        }

        while (!targets.empty() && totalWeight > 0)
        {
            // pick a random value
            auto const value = rand() % totalWeight;

            // find anything that is above the threshold
            Configuration::Pool::Entry const *target = nullptr;
            auto weightSum = 0;
            for (auto const candidate : targets)
                if (weightSum += candidate->weight; weightSum > value)
                {
                    target = candidate;
                    break;
                }

            // remove the selected thing from the pool unrelated to placement successfulness
            targets.erase(target);
            totalWeight -= target->weight;

            // attempt to place a random variant of the thing
            auto const &structureId = target->structureVariants[rand() % target->structureVariants.size()];
            auto const newReq = try_request_structure(
                structureId,
                targetJointX, targetJointY, -joint->direction[0], -joint->direction[1], joint->tag,
                budget,
                checkContinuity);

            if (newReq)
                return newReq;
        }

        // there are no structures had been chosen so far - switching to the backup pool
        pool = structureProvider->get_pool(pool->fallback);
    }

    return nullptr;
}

StructureBuilder::StructureBuilder()
{
    placementCheckers.emplace("underground", PlacementCheckers::underground);
    placementCheckers.emplace("no-tiles", PlacementCheckers::no_tiles);
}

void StructureBuilder::attach_world(World *const worldPtr)
{
    this->world = worldPtr;
}

void StructureBuilder::attach_structure_provider(StructureProvider *const provider)
{
    this->structureProvider = provider;
}

void StructureBuilder::attach_tile_registry(TileRegistry const *registry)
{
    this->tileRegistry = registry;
}

void StructureBuilder::reset()
{
    std::fill(std::begin(obstructed), std::end(obstructed), false);
    std::fill(std::begin(reserved), std::end(reserved), false);
    jointMap.clear();
}

void StructureBuilder::request_structure(
    std::string const &structureId,
    int jointWorldX,
    int jointWorldY,
    std::string const &jointTag,
    int budget)
{
    auto const newReq = try_request_structure(structureId, jointWorldX, jointWorldY, 0, 0, jointTag, budget, false);
    if (!newReq)
        return;

    // schedule the placement
    buildQueue.emplace_back(newReq);

    // claim space and pre-register joints
    claim_space(newReq->x, newReq->y, newReq->obj, obstructed);
    place_joints(newReq->x, newReq->y, newReq->obj);
}

using JointCRef = Configuration::Structure::Joint const *;

inline auto structure_joint_comparator(JointCRef a, JointCRef b)
{
    int const ha = Configuration::location_hash(a->location[0], a->location[1]);
    int const hb = Configuration::location_hash(b->location[0], b->location[1]);
    return ha - hb;
}

StructureBuilder::BuildRequest *StructureBuilder::try_request_structure(
    std::string const &structureId,
    int expectedJointWorldX,
    int expectedJointWorldY,
    int targetJointDirX,
    int targetJointDirY,
    std::string const &targetTag,
    int budget,
    bool checkContinuity)
{
    // find the structure and correct the cost of current building branch
    auto const obj = structureProvider->get_structure(structureId);
    budget -= obj->config.cost;
    if (budget < 1)
        return nullptr;

    // look for suitable target joints
    std::deque<JointCRef> joints;
    for (auto const &[_, joint] : obj->config.joints)
        if (joint.direction[0] == targetJointDirX && joint.direction[1] == targetJointDirY &&
            joint.tag == targetTag)
            joints.emplace_back(&joint);

    if (joints.empty())
        return nullptr;

    // make some sort of order (implementation-independent) in deciding what joint to choose
    if (joints.size() != 1)
        std::sort(joints.begin(), joints.end(), structure_joint_comparator);

    // try every target one by one
    while (!joints.empty())
    {
        auto const joint = joints.front();
        joints.pop_front();

        // correct the origin point
        auto const ox = expectedJointWorldX - joint->location[0];
        auto const oy = expectedJointWorldY - obj->height + 1 + joint->location[1];

        // can this be placed at this exact location?
        if (is_in_world(ox, oy, obj) &&
            is_compatible(ox, oy, obj) &&
            is_free_space(ox, oy, obj, obstructed) &&
            is_satisfied(ox, oy, obj))
        {
            if (checkContinuity)
            {
                if (!is_continuable(ox, oy, obj))
                    continue;
            }
            else
            {
                if (!is_free_space(ox, oy, obj, reserved))
                    continue;
            }

            // schedule the placement
            auto const req = requestPool.get();
            req->x = ox;
            req->y = oy;
            req->obj = obj;
            req->budget = budget;

            // successive "placement"
            return req;
        }
    }

    return nullptr;
}

void StructureBuilder::process_all_requests()
{
    while (!buildQueue.empty())
    {
        auto const request = buildQueue.front();
        buildQueue.pop_front();

        // materialize the thing and propagate ongoing structures further after its joints
        build(request);
        propagate(request);

        // free the object
        requestPool.release(request);
    }
}

bool StructureBuilder::step()
{
    printf("Requests: %zd\n", buildQueue.size());
    if (!buildQueue.empty())
    {
        auto const request = buildQueue.front();
        buildQueue.pop_front();

        // materialize the thing and propagate ongoing structures further after its joints
        build(request);
        propagate(request);

        // free the object
        requestPool.release(request);

        return true;
    }

    return false;
}

// ========================================================================

inline static float fractal_noise(int octaves, float x, float y = 0.f, float z = 0.f)
{
    auto result = 0.f;
    auto scale = 1.f;
    auto k = 0.5f;

    for (int i = 0; i < octaves; i++)
    {
        scale *= 0.5f;
        k *= 2.f;

        result += scale * (noise3(x * k, y * k, z * k) + 1.f) * 0.5f;
    }

    return result;
}

void WorldGenerator::gen_soil(World *const world)
{
    auto const z = rand() % 256;

    for (int y = 0; y < WORLD_HEIGHT; y++)
        for (int x = 0; x < WORLD_WIDTH; x++)
        {
            auto const n1 = fractal_noise(3, x / 128.f, z);
            auto const n2 = fractal_noise(3, x / 64.f, y / 64.f, z) * (WORLD_HEIGHT - y) / WORLD_HEIGHT;
            auto const n3 = fractal_noise(2, x / 32.f, y / 16.f, z + 1.f);

            if (n3 > 0.385f * 0.85f)
            {
                if (y < n1 * WORLD_HEIGHT)
                    world->set_tile(x, y, Tiles::SOIL);

                if (n2 > 0.3f)
                    world->set_tile(x, y, Tiles::STONE);
            }
            /*if (y < (WORLD_HEIGHT >> 1))
                world->set_tile(x, y, Tiles::STONE);*/
        }
}

void WorldGenerator::gen_base(World *const world)
{
    int const startX = 21 + rand() % (WORLD_WIDTH_M1 - 21 * 2);
    int const startY = world->get_height_at(startX);

    builder.request_structure("room/base", startX, startY, "#start", 80);
    // builder.process_all_requests();
}

void WorldGenerator::generate(World *const world, TileRegistry const *const tileRegistry)
{
    provider.attach_tile_registry(tileRegistry);

    builder.reset();
    builder.attach_tile_registry(tileRegistry);
    builder.attach_structure_provider(&provider);
    builder.attach_world(world);

    gen_soil(world);

    for (int i = 0; i < 5; i++)
    gen_base(world);
}

bool WorldGenerator::step()
{
    return builder.step();
}
