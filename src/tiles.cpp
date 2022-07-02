#include "tiles.hpp"

TileId TileRegistry::getTile(std::string const &name) const
{
    if (auto const iter = names.find(name); iter != names.cend())
        return iter->second;
    else
        return UNKNOWN;
}

Color TileRegistry::getTileColor(TileId tile) const
{
    if (auto const iter = colors.find(tile); iter != colors.cend())
        return iter->second;
    else
        return RED;
}

void TileRegistry::registerTile(TileId const tile, std::string const &name, Color color)
{
    color.a = 255;
    names[name] = tile;
    colors[tile] = color;
}
