#pragma once

#include <unordered_map>
#include <raylib.h>

typedef uint8_t TileId;

namespace Tiles
{
    constexpr TileId AIR = 0;
    constexpr TileId SOIL = 1;
    constexpr TileId STONE = 2;

    constexpr TileId BACKGROUND = 3;
    constexpr TileId WALL = 4;
    constexpr TileId CHAIN = 5;

    constexpr TileId STRUCTURE_VOID = 128;
    constexpr TileId STRUCTURE_JOINT = 129;

    constexpr TileId UNKNOWN = 255;
}

class TileRegistry
{
private:
    std::unordered_map<TileId, Color> colors;
    std::unordered_map<std::string, TileId> names;

public:
    TileId get_tile(std::string const &name) const;
    Color get_tile_color(TileId const tile) const;

    void register_tile(TileId const tile, std::string const &name, Color color);
};
