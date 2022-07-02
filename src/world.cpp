#include "world.hpp"

void World::setTile(int x, int y, TileId tile)
{
    if (x < 0 || x > WORLD_WIDTH_M1 ||
        y < 0 || y > WORLD_HEIGHT_M1)
        return;

    tiles[x + y * WORLD_WIDTH] = tile;
    auto &height = heightMap[x];

    if (tile == AIR)
    {
        if (y == height)
            while (height > 0 && tiles[x + height * WORLD_WIDTH] == AIR)
                --height;
    }
    else
    {
        if (height < y)
            height = y;
    }
}

TileId World::getTileAt(int x, int y) const
{
    if (x < 0 || x > WORLD_WIDTH_M1 ||
        y < 0 || y > WORLD_HEIGHT_M1)
        return AIR;

    return tiles[x + y * WORLD_WIDTH];
}

int World::getHeightAt(int const x) const
{
    if (x < 0 || x > WORLD_WIDTH_M1)
        return 0;
    else
        return heightMap[x];
}

void World::clear()
{
    std::fill(std::begin(tiles), std::end(tiles), AIR);
    std::fill(std::begin(heightMap), std::end(heightMap), 0);
}

void World::render(Image *const img, TileRegistry const *const registry) const
{
    auto tilePtr = tiles + WORLD_HEIGHT_M1 * WORLD_WIDTH;
    auto pixel = (Color *)img->data;

    for (int y = 0; y < WORLD_HEIGHT; y++, tilePtr -= WORLD_WIDTH * 2)
        for (int x = 0; x < WORLD_WIDTH; x++, tilePtr++, pixel++)
            *pixel = registry->getTileColor(*tilePtr);
}
