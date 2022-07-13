#include "world.hpp"

void World::set_tile(int x, int y, TileId tile)
{
    if (x < 0 || x > WORLD_WIDTH_M1 ||
        y < 0 || y > WORLD_HEIGHT_M1)
        return;

    tiles[x + y * WORLD_WIDTH] = tile;
    auto &height = heightMap[x];

    if (tile == Tiles::AIR)
    {
        if (y == height)
            while (height > 0 && tiles[x + height * WORLD_WIDTH] == Tiles::AIR)
                --height;
    }
    else
    {
        if (height < y)
            height = y;
    }
}

TileId World::get_tile_at(int x, int y) const
{
    if (x < 0 || x > WORLD_WIDTH_M1 ||
        y < 0 || y > WORLD_HEIGHT_M1)
        return Tiles::AIR;

    return tiles[x + y * WORLD_WIDTH];
}

int World::get_height_at(int const x) const
{
    if (x < 0 || x > WORLD_WIDTH_M1)
        return 0;
    else
        return heightMap[x];
}

void World::clear()
{
    std::fill(std::begin(tiles), std::end(tiles), Tiles::AIR);
    std::fill(std::begin(heightMap), std::end(heightMap), 0);
}

void World::render(Image *const img, TileRegistry const *const registry) const
{
    auto tilePtr = tiles + WORLD_HEIGHT_M1 * WORLD_WIDTH;
    auto pixel = (Color *)img->data;

    for (int y = 0; y < WORLD_HEIGHT; y++, tilePtr -= WORLD_WIDTH * 2)
        for (int x = 0; x < WORLD_WIDTH; x++, tilePtr++, pixel++)
            *pixel = registry->get_tile_color(*tilePtr);
}
