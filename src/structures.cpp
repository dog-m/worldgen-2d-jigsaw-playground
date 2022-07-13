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
        json.at("fallback").get_to(pool.fallback);
        json.at("structures").get_to(pool.structures);
    }

    void from_json(const nlohmann::json &json, Structure::Joint &joint)
    {
        json.at("tag").get_to(joint.tag);
        json.at("location").get_to(joint.location);
        json.at("direction").get_to(joint.direction);
        json.at("replace-by").get_to(joint.replaceBy);
        json.at("pool").get_to(joint.structurePool);
    }

    void from_json(const nlohmann::json &json, Structure &structure)
    {
        json.at("cost").get_to(structure.cost);
        json.at("placement-constraints").get_to(structure.placementConstraints);

        auto const joints = json.at("joints").get<std::vector<Structure::Joint>>();
        for (auto &joint : joints)
        {
            auto const coords = Configuration::location_hash(joint.location[0], joint.location[1]);
            structure.joints.emplace(coords, std::move(joint));
        }

        auto const mapping = json.at("color-to-tile-mapping")
                                 .get<std::unordered_map<std::string, std::array<uint32_t, 3>>>();
        for (auto const [name, rgb] : mapping)
        {
            auto const [r, g, b] = rgb;
            structure.colorsToBlocks.emplace(RGB_COLOR(r, g, b), name);
        }
    }
}

// ========================================================================

static std::string const DIR_POOLS = "../../res/pools/";
static std::string const DIR_STRUCTURES = "../../res/structures/";

StructureObject const *StructureProvider::load_structure(std::string const &id)
{
    auto p = loadedStructures.emplace(id, std::make_unique<StructureObject>());
    auto result = p.first->second.get();

    // load the configuration
    try
    {
        std::ifstream in(DIR_STRUCTURES + id + ".json");
        nlohmann::json::parse(in).get_to(result->config);
    }
    catch (const nlohmann::json::exception &e)
    {
        printf("[X] Invalid structure <%s>. Reason: %s\n", id.c_str(), e.what());
    }

    // load image
    auto img = LoadImage((DIR_STRUCTURES + id + ".png").c_str());
    result->width = img.width;
    result->height = img.height;

    // change orientation before-hand for ease of use
    ImageFlipVertical(&img);
    ImageFormat(&img, PixelFormat::UNCOMPRESSED_R8G8B8A8);
    // convert to tiles

    result->tiles = std::vector<TileId>(img.width * img.height, Tiles::AIR);

    auto pixel = (Color const *)img.data;
    auto tile = result->tiles.data();
    auto const tileLast = tile + result->tiles.size();
    for (; tile != tileLast; pixel++, tile++)
    {
        auto const color = RGB_COLOR(pixel->r, pixel->g, pixel->b);
        auto const name = result->config.colorsToBlocks[color];
        *tile = tileRegistry->get_tile(name);
    }

    UnloadImage(img);

    return result;
}

Configuration::Pool const *StructureProvider::load_pool(std::string const &id)
{
    auto p = loadedPools.emplace(id, std::make_unique<Configuration::Pool>());
    auto result = p.first->second.get();

    // load the configuration
    std::ifstream in(DIR_POOLS + id + ".json");
    nlohmann::json::parse(in).get_to(*result);

    // pre-load fallbacks
    get_pool(result->fallback);

    return result;
}

void StructureProvider::attach_tile_registry(TileRegistry const *const registry)
{
    this->tileRegistry = registry;
}

StructureObject const *StructureProvider::get_structure(std::string const &id)
{
    if (auto const iter = loadedStructures.find(id); iter != loadedStructures.cend())
        return iter->second.get();
    else
        return load_structure(id);
}

Configuration::Pool const *StructureProvider::get_pool(std::string const &id)
{
    if (auto const iter = loadedPools.find(id); iter != loadedPools.cend())
        return iter->second.get();
    else
        return load_pool(id);
}
