#include <iostream>

#include <raylib.h>

#include "structures.hpp"
#include "tiles.hpp"
#include "world.hpp"
#include "worldgen.hpp"

int main(void)
{
    const int screenWidth = 1000;
    const int screenHeight = 350;

    // init a new window and set expected frame rate
    InitWindow(screenWidth, screenHeight, "2D Worldgen Testing Ground");
    SetTargetFPS(20);

    auto const tiles = std::make_unique<TileRegistry>();
    tiles->registerTile(AIR, "blocks/air", BLACK);
    tiles->registerTile(SOIL, "blocks/soil", DARKPURPLE);
    tiles->registerTile(STONE, "blocks/stone", DARKBLUE);
    tiles->registerTile(BACKGROUND, "blocks/background", DARKGRAY);
    tiles->registerTile(WALL, "blocks/wall", LIGHTGRAY);
    tiles->registerTile(CHAIN, "blocks/chain", LIME);

    tiles->registerTile(STRUCTURE_VOID, "structure/void", RED);
    tiles->registerTile(STRUCTURE_JOINT, "structure/joint", RED);

    auto const world = std::make_unique<World>();
    auto const gen = std::make_unique<WorldGenerator>();

    auto imgWorld = GenImageColor(WORLD_WIDTH, WORLD_HEIGHT, BLACK);
    auto texWorld = LoadTextureFromImage(imgWorld);

    bool mouseIsHoldingDown = false;
    Vector2 mouseHoldPos{0}, mouseCurrentPos{0};

    Vector2 worldOffset{0};
    auto worldScale = 1.f;

    // main loop
    while (!WindowShouldClose())
    {
        // update world image scaling
        worldScale += 0.0625f * GetMouseWheelMove();

        // update world image offset
        if (IsMouseButtonDown(MouseButton::MOUSE_LEFT_BUTTON))
        {
            if (mouseIsHoldingDown)
            {
                mouseCurrentPos = GetMousePosition();

                worldOffset.x = mouseCurrentPos.x - mouseHoldPos.x;
                worldOffset.y = mouseCurrentPos.y - mouseHoldPos.y;
            }
            else
            {
                mouseIsHoldingDown = true;
                mouseHoldPos = GetMousePosition();

                mouseHoldPos.x -= worldOffset.x;
                mouseHoldPos.y -= worldOffset.y;
            }
        }
        else
        {
            mouseIsHoldingDown = false;
        }

        // world re-generation handling
        if (IsKeyPressed(KeyboardKey::KEY_SPACE))
        {
            world->clear();

            gen->generate(world.get(), tiles.get());
            world->render(&imgWorld, tiles.get());

            UpdateTexture(texWorld, imgWorld.data);
        }

        // draw the world
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextureEx(texWorld, worldOffset, 0.f, worldScale, WHITE);
        EndDrawing();
    }

    // free common resources
    UnloadTexture(texWorld);
    UnloadImage(imgWorld);
    CloseWindow();

    return 0;
}
