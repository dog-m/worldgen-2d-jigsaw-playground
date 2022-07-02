#include "structures.hpp"

#include <raylib.h>
#include <fstream>

#include "tiles.hpp"

namespace Configuration
{
    void from_json(const nlohmann::json &json, Pool::Entry &entry)
    {
        json.at("weight").get_to(entry.weight);
        json.at("variants").get_to(entry.structureVariants);
    }

    void from_json(const nlohmann::json &json, Pool &pool)
    {
        json.at("fallback").get_to(pool.fallbackPool);
        json.at("structures").get_to(pool.structures);
    }

    void from_json(const nlohmann::json &json, Structure::Joint::Target &target)
    {
        json.at("id").get_to(target.structureId);
        json.at("joint").get_to(target.joint);
        json.at("weight").get_to(target.weight);
    }

    void from_json(const nlohmann::json &json, Structure::Joint &joint)
    {
        json.at("location").get_to(joint.location);
        json.at("direction").get_to(joint.direction);
        json.at("replace-by").get_to(joint.replaceBy);
        json.at("structures").get_to(joint.structures);
    }

    void from_json(const nlohmann::json &json, Structure &structure)
    {
        json.at("cost").get_to(structure.cost);
        json.at("joints").get_to(structure.joints);
        json.at("placement-constraints").get_to(structure.placementConstraints);

        auto const mapping = json.at("color-to-tile-mapping")
                                 .get<std::unordered_map<std::string, std::array<uint32_t, 3>>>();
        for (auto const [name, rgb] : mapping)
        {
            auto const [r, g, b] = rgb;
            auto const color = (r << 16) + (g << 8) + b;
            structure.colorsToBlocks.emplace(color, name);
        }

        // post-processing
        for (auto const [name, joint] : structure.joints)
        {
            auto const coords = (joint.location[0] << 8) + joint.location[1];
            structure.coordToJoint.emplace(coords, name);
        }
    }
}

// ========================================================================

static std::string const DIR_POOLS = "../../res/pools/";
static std::string const DIR_STRUCTURES = "../../res/structures/";

StructureObject const *StructureProvider::loadStructure(std::string const &id)
{
    auto p = loadedStructures.emplace(id, std::make_unique<StructureObject>());
    auto result = p.first->second.get();

    // load the configuration
    std::ifstream in(DIR_STRUCTURES + id + ".json");
    nlohmann::json::parse(in).get_to(result->config);

    // load image
    auto img = LoadImage((DIR_STRUCTURES + id + ".png").c_str());
    result->width = img.width;
    result->height = img.height;

    // change orientation before-hand for ease of use
    ImageFlipVertical(&img);
    ImageFormat(&img, PixelFormat::UNCOMPRESSED_R8G8B8A8);
    // convert to tiles

    result->tiles = std::vector<TileId>(img.width * img.height, AIR);

    auto pixel = (Color const *)img.data;
    auto tile = result->tiles.data();
    auto const tileLast = tile + result->tiles.size();
    for (; tile != tileLast; pixel++, tile++)
    {
        auto const color = (pixel->r << 16) + (pixel->g << 8) + pixel->b;
        auto const name = result->config.colorsToBlocks[color];
        *tile = tileRegistry->getTile(name);
    }

    UnloadImage(img);

    return result;
}

Configuration::Pool const *StructureProvider::loadPool(std::string const &id)
{
    auto p = loadedPools.emplace(id, std::make_unique<Configuration::Pool>());
    auto result = p.first->second.get();

    // load the configuration
    std::ifstream in(DIR_POOLS + id + ".json");
    nlohmann::json::parse(in).get_to(*result);

    // pre-load fallbacks
    getPool(result->fallbackPool);

    return result;
}

void StructureProvider::attachTileRegistry(TileRegistry const *const registry)
{
    this->tileRegistry = registry;
}

StructureObject const *StructureProvider::getStructure(std::string const &id)
{
    if (auto const iter = loadedStructures.find(id); iter != loadedStructures.cend())
        return iter->second.get();
    else
        return loadStructure(id);
}

Configuration::Pool const *StructureProvider::getPool(std::string const &id)
{
    if (auto const iter = loadedPools.find(id); iter != loadedPools.cend())
        return iter->second.get();
    else
        return loadPool(id);
}
