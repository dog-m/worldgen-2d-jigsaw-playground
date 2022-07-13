#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "tiles.hpp"

#define RGB_COLOR(r, g, b) ((r) << 16) | ((g) << 8) | (b)

namespace Configuration
{

    struct Pool
    {
        struct Entry
        {
            int32_t weight;
            std::vector<std::string> structureVariants;
        };

        std::string fallback;
        std::vector<Entry> structures;
    };

    using LocationHash = uint32_t;

    inline LocationHash location_hash(int32_t x, int32_t y)
    {
        return x << 8 | y;
    }

    struct Structure
    {

        struct Joint
        {
            std::string tag;
            std::array<uint16_t, 2> location;
            std::array<int16_t, 2> direction;
            std::string replaceBy;
            std::string structurePool;

            inline bool isFacing(Joint const &other) const
            {
                return this->direction[0] == -other.direction[0] &&
                       this->direction[1] == -other.direction[1];
            }

            inline bool isCompatibleWith(Joint const &other) const
            {
                return this->tag == other.tag;
            }
        };

        uint32_t cost;
        std::unordered_map<LocationHash, Joint> joints;
        std::unordered_set<std::string> placementConstraints;
        std::unordered_map<uint32_t, std::string> colorsToBlocks;
    };

    void from_json(const nlohmann::json &json, Pool::Entry &entry);

    void from_json(const nlohmann::json &json, Pool &pool);

    void from_json(const nlohmann::json &json, Structure::Joint &joint);

    void from_json(const nlohmann::json &json, Structure &structure);
}

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
    std::unordered_map<std::string, std::unique_ptr<Configuration::Pool>> loadedPools;
    std::unordered_map<std::string, std::unique_ptr<StructureObject>> loadedStructures;
    TileRegistry const *tileRegistry = nullptr;

    StructureObject const *load_structure(std::string const &id);

    Configuration::Pool const *load_pool(std::string const &id);

public:
    void attach_tile_registry(TileRegistry const *const registry);

    StructureObject const *get_structure(std::string const &id);

    Configuration::Pool const *get_pool(std::string const &id);
};
