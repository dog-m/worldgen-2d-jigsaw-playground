#include "worldgen.hpp"

extern "C"
{
#include "thirdparty/noise/noise1234.h"
}

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

void StructureBuilder::claimStructureSpace(
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

bool StructureBuilder::can_be_built(
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

void StructureBuilder::build(
    int const callerX,
    int const callerY,
    StructureObject const *const obj,
    int const cost)
{
    auto tilePtr = obj->tiles.data();

    // materialize the structure
    for (int y = 0; y < obj->height; y++)
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

void StructureBuilder::propagate(
    int const callerX,
    int const callerY,
    StructureObject const *const obj,
    int const cost)
{
    std::unordered_set<Configuration::Structure::Joint::Target const *> targets;

    for (auto const [_, joint] : obj->config.joints)
        if (joint.structures.size() > 0)
        {
            // queue the following structure (structure-specific alignment will be done separately)
            auto const newX = callerX + joint.direction[0] + joint.location[0];
            auto const newY = callerY + joint.direction[1] + (obj->height - 1 - joint.location[1]);

            // prepare the pool of target structures
            targets.clear();
            auto totalWeight = 0;
            for (auto const &target : joint.structures)
            {
                targets.emplace(&target);
                totalWeight += target.weight;
            }

            // enqueue the random placeable target
            while (!targets.empty())
            {
                Configuration::Structure::Joint::Target const *target = nullptr;

                if (totalWeight == 0 || targets.size() == 1)
                {
                    // pick the only thing left
                    target = *targets.cbegin();
                }
                else
                {
                    // pick a random value
                    auto const value = rand() % totalWeight;

                    // find anything that is above the threshold
                    auto weightSum = 0;
                    for (auto const candidate : targets)
                        if (weightSum += candidate->weight; weightSum > value)
                        {
                            target = candidate;
                            break;
                        }
                }

                // remove the selected thing from the pool unrelated to placement successfulness
                targets.erase(target);
                totalWeight -= target->weight;

                // attempt to place the thing
                if (requestStructureAt(newX, newY, target->structureId, target->joint, cost))
                    break;
            }
        }
}

StructureBuilder::StructureBuilder()
{
    this->placementCheckers.emplace("underground", undergroundPlacementChecker);
    this->placementCheckers.emplace("no-blocks", noBlocksPlacementChecker);
}

void StructureBuilder::attachWorld(World *const worldPtr)
{
    this->world = worldPtr;
}

void StructureBuilder::attachStructureProvider(StructureProvider *const provider)
{
    this->structureProvider = provider;
}

void StructureBuilder::attachTileRegistry(TileRegistry const *registry)
{
    this->tileRegistry = registry;
}

void StructureBuilder::reset()
{
    std::fill(std::begin(obstructed), std::end(obstructed), false);
}

bool StructureBuilder::requestStructureAt(
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
    if (!can_be_built(x, y, obj))
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

void StructureBuilder::processAllRequests()
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

// ========================================================================

inline float WorldGenerator::fractalNoise(int octaves, float x, float y, float z)
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

void WorldGenerator::genSoil(World *const world)
{
    auto const z = rand() % 256;

    for (int y = 0; y < WORLD_HEIGHT; y++)
        for (int x = 0; x < WORLD_WIDTH; x++)
        {
            auto const n1 = fractalNoise(3, x / 128.f, z);
            auto const n2 = fractalNoise(3, x / 64.f, y / 64.f, z) * (WORLD_HEIGHT - y) / WORLD_HEIGHT;
            auto const n3 = fractalNoise(2, x / 32.f, y / 16.f, z + 1.f);

            if (n3 > 0.385f * 0.85f)
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

void WorldGenerator::genBase(World *const world)
{
    int const startX = 16 + rand() % (WORLD_WIDTH_M1 - 16 * 2);
    int const startY = world->getHeightAt(startX);

    builder.requestStructureAt(startX, startY, "room/base", "#floor", 0);
    builder.processAllRequests();
}

void WorldGenerator::generate(World *const world, TileRegistry const *const tileRegistry)
{
    provider.attachTileRegistry(tileRegistry);

    builder.reset();
    builder.attachTileRegistry(tileRegistry);
    builder.attachStructureProvider(&provider);
    builder.attachWorld(world);

    genSoil(world);
    // genBase(world);
}
