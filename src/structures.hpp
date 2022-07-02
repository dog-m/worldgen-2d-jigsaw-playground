#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "tiles.hpp"

namespace Configuration
{
    struct Pool
    {
        struct Entry
        {
            int32_t weight;
            std::vector<std::string> structureVariants;
        };

        std::string fallbackPool;
        std::vector<Entry> structures;
    };

    struct Structure
    {
        struct Joint
        {
            struct Target
            {
                std::string structureId;
                std::string joint;
                int32_t weight;
            };

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

    void from_json(const nlohmann::json &json, Pool::Entry &entry);

    void from_json(const nlohmann::json &json, Pool &pool);

    void from_json(const nlohmann::json &json, Structure::Joint::Target &target);

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

    StructureObject const *loadStructure(std::string const &id);

    Configuration::Pool const *loadPool(std::string const &id);

public:
    void attachTileRegistry(TileRegistry const *const registry);

    StructureObject const *getStructure(std::string const &id);

    Configuration::Pool const *getPool(std::string const &id);
};
