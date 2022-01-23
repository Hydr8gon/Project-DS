/*
    Copyright 2022 Hydr8gon

    This file is part of Project DS.

    Project DS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published
    by the Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    Project DS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Project DS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <fat.h>
#include <nds.h>

#include <cstdint>
#include <cstdio>
#include <string>

#include "circle.h"
#include "cross.h"
#include "square.h"
#include "triangle.h"

uint16_t *gfx[4];
int palIdx[4];

uint32_t counter = 1;

const uint8_t paramCounts[0x100] =
{
    0,  1,  4,  2,  2,  2,  7,  4,  2,  6,  2,  1,  6,  2,  1,  1,
    3,  2,  3,  5,  5,  4,  4,  5,  2,  0,  2,  4,  2,  2,  1, 21,
    0,  3,  2,  5,  1,  1,  7,  1,  1,  2,  1,  2,  1,  2,  3,  3,
    1,  2,  2,  3,  6,  6,  1,  1,  2,  3,  1,  2,  2,  4,  4,  1,
    2,  1,  2,  1,  1,  3,  3,  3,  2,  1,  9,  3,  2,  4,  2,  3,
    2, 24,  1,  2,  1,  3,  1,  3,  4,  1,  2,  6,  3,  2,  3,  3,
    4,  1,  1,  3,  3,  4,  2,  3,  3,  8,  2
};

const uint16_t keys[4] =
{
    (KEY_X | KEY_UP),   (KEY_A | KEY_RIGHT), // Triangle, Circle
    (KEY_B | KEY_DOWN), (KEY_Y | KEY_LEFT)   // Cross,    Square
};

uint16_t *initObjTiles16(const unsigned int *tiles, size_t tilesLen, SpriteSize size)
{
    // Copy 16-color object tiles into appropriate memory, and return a pointer to the data
    uint16_t *gfx = oamAllocateGfx(&oamMain, size, SpriteColorFormat_16Color);
    if (gfx) dmaCopy(tiles, gfx, tilesLen);
    return gfx;
}

int initObjPal16(const unsigned short *pal)
{
    // Copy a 16-color object palette into appropriate memory, and return the palette index
    static uint8_t index = 0;
    if (index >= 32) return -1;
    dmaCopy(pal, &SPRITE_PALETTE[index * 16], 16 * sizeof(uint16_t));
    return index++;
}

void retry()
{
    printf("Press start to retry.\n");

    // Wait until start is pressed to reset the chart
    while (true)
    {
        scanKeys();

        if (keysDown() & KEY_START)
        {
            consoleClear();
            counter = 1;
            return;
        }

        swiWaitForVBlank();
    }
}

int main()
{
    fatInitDefault();
    consoleDemoInit();

    // Setup graphics on the main screen
    videoSetMode(MODE_3_2D);
    vramSetBankA(VRAM_A_MAIN_SPRITE);
    oamInit(&oamMain, SpriteMapping_1D_64, false);
    BG_PALETTE[0] = 0xFFFF;

    size_t size = 0;
    uint32_t *chart = nullptr;

    if (FILE *file = fopen("chart.dsc", "rb"))
    {
        // Load the chart file into memory
        fseek(file, 0, SEEK_END);
        size = ftell(file) / 4;
        fseek(file, 0, SEEK_SET);
        chart = new uint32_t[size];
        fread(chart, sizeof(uint32_t), size, file);
        fclose(file);
    }
    else
    {
        printf("Please provide a file named chart.dsc on the SD card.\n");

        // Do nothing since there's no chart to play
        while (true)
            swiWaitForVBlank();
    }

    // Prepare the graphic tile data
    gfx[0] = initObjTiles16(triangleTiles, triangleTilesLen, SpriteSize_32x32);
    gfx[1] = initObjTiles16(circleTiles,   circleTilesLen,   SpriteSize_32x32);
    gfx[2] = initObjTiles16(crossTiles,    crossTilesLen,    SpriteSize_32x32);
    gfx[3] = initObjTiles16(squareTiles,   squareTilesLen,   SpriteSize_32x32);

    // Prepare the graphic palette data
    palIdx[0] = initObjPal16(trianglePal);
    palIdx[1] = initObjPal16(circlePal);
    palIdx[2] = initObjPal16(crossPal);
    palIdx[3] = initObjPal16(squarePal);

    // Execute chart opcodes
    while (counter < size)
    {
        switch (chart[counter])
        {
            case 0x00: // End
            {
                printf("CLEAR!\n");
                retry();
                break;
            }

            case 0x06: // Target
            {
                uint16_t key = 0;
                uint16_t down = 0;
                int sprite = 0;

                oamClear(&oamMain, 0, 0);

                while (chart[counter] == 0x06)
                {
                    uint32_t note = chart[counter + 1];
                    uint32_t x = chart[counter + 2] * 256 / 500000 - 16;
                    uint32_t y = chart[counter + 3] * 192 / 250000 - 16;

                    if (note < 8) // Buttons and holds
                    {
                        // Add the note to the keypress; it could be a multi-note
                        key |= keys[note & 3];

                        // Draw the note on the top screen
                        oamSet(&oamMain, sprite++, x, y, 0, palIdx[note & 3], SpriteSize_32x32,
                            SpriteColorFormat_16Color, gfx[note & 3], 0, false, false, false, false, false);
                    }

                    // Move to the next opcode
                    counter += paramCounts[0x06] + 1;
                }

                oamUpdate(&oamMain);

                if (key)
                {
                    // Wait for a key to be pressed
                    while (!down)
                    {
                        scanKeys();
                        down = keysDown();
                        swiWaitForVBlank();
                    }

                    // Check if the key is correct
                    if (!(down & key))
                    {
                        printf("FAILED...\n");
                        retry();
                    }
                }

                break;
            }

            default:
            {
                // Move to the next opcode
                counter += paramCounts[chart[counter]] + 1;
                break;
            }
        }
    }

    return 0;
}
