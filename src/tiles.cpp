#include "tiles.hpp"

TileId TileRegistry::get_tile(std::string const &name) const
{
    if (auto const iter = names.find(name); iter != names.cend())
        return iter->second;
    else
        return Tiles::UNKNOWN;
}

Color TileRegistry::get_tile_color(TileId const tile) const
{
    if (auto const iter = colors.find(tile); iter != colors.cend())
        return iter->second;
    else
        return PINK;
}

void TileRegistry::register_tile(TileId const tile, std::string const &name, Color color)
{
    color.a = 255;
    names[name] = tile;
    colors[tile] = color;
}
