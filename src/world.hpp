#pragma once

#include <stdint.h>
#include "tiles.hpp"

constexpr int WORLD_WIDTH = 1000;
constexpr int WORLD_WIDTH_M1 = WORLD_WIDTH - 1;

constexpr int WORLD_HEIGHT = 350;
constexpr int WORLD_HEIGHT_M1 = WORLD_HEIGHT - 1;

class World
{
private:
    TileId tiles[WORLD_WIDTH * WORLD_HEIGHT] = {0};
    uint16_t heightMap[WORLD_WIDTH] = {0};

public:
    void set_tile(int x, int y, TileId tile);
    TileId get_tile_at(int x, int y) const;
    int get_height_at(int const x) const;

    void clear();

    void render(Image *const img, TileRegistry const *const registry) const;
};
