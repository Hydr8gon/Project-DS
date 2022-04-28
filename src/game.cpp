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

#include <cmath>
#include <cstdint>
#include <deque>

#include <nds.h>

#include "circle.h"
#include "circle_hole.h"
#include "cross.h"
#include "cross_hole.h"
#include "square.h"
#include "square_hole.h"
#include "triangle.h"
#include "triangle_hole.h"
#include "slider_l.h"
#include "slider_lh.h"
#include "slider_l_hole.h"
#include "slider_lh_hole.h"
#include "slider_r.h"
#include "slider_rh.h"
#include "slider_r_hole.h"
#include "slider_rh_hole.h"
#include "arrow.h"
#include "cool.h"
#include "fine.h"
#include "safe.h"
#include "sad.h"
#include "miss.h"
#include "combo_nums.h"

#include "game.h"
#include "audio.h"
#include "menu.h"

#define PI 3.14159
#define FRAME_TIME 1672

struct Note
{
    uint8_t type;
    int32_t x, y;
    int32_t incX, incY;
    int32_t ofsX, ofsY;
    uint16_t incArrow;
    uint16_t ofsArrow;
    uint32_t time;
};

static std::deque<Note> notes;

static uint16_t *gameGfx[22];
static uint16_t *numGfx[10];

static std::string songName;

static size_t chartSize = 0;
static uint32_t *chart = nullptr;
static uint32_t flyTimeDef = 1750;

static uint32_t counter = 1;
static uint32_t timer = 0;
static uint32_t flyTime = flyTimeDef * 100;
static bool finished = false;

static uint8_t current = 0;
static uint8_t mask = 0;
static uint8_t mask2 = 0;
static uint8_t statTimer = 0;

static uint32_t combo = 0;
static uint8_t life = 127;

static const uint8_t paramCounts[0x100] =
{
    0,  1,  4,  2,  2,  2,  7,  4,  2,  6,  2,  1,  6,  2,  1,  1, // 0x00-0x0F
    3,  2,  3,  5,  5,  4,  4,  5,  2,  0,  2,  4,  2,  2,  1, 21, // 0x10-0x1F
    0,  3,  2,  5,  1,  1,  7,  1,  1,  2,  1,  2,  1,  2,  3,  3, // 0x20-0x2F
    1,  2,  2,  3,  6,  6,  1,  1,  2,  3,  1,  2,  2,  4,  4,  1, // 0x30-0x3F
    2,  1,  2,  1,  1,  3,  3,  3,  2,  1,  9,  3,  2,  4,  2,  3, // 0x40-0x4F
    2, 24,  1,  2,  1,  3,  1,  3,  4,  1,  2,  6,  3,  2,  3,  3, // 0x50-0x5F
    4,  1,  1,  3,  3,  4,  2,  3,  3,  8,  2                      // 0x60-0x6A
};

static const uint16_t keys[6] =
{
    (KEY_X | KEY_UP),   (KEY_A | KEY_RIGHT), // Triangle, Circle
    (KEY_B | KEY_DOWN), (KEY_Y | KEY_LEFT),  // Cross,    Square
    KEY_L,              KEY_R                // Slider L, Slider R
};

static uint16_t *initObjBitmap(const unsigned int *bitmap, size_t bitmapLen, SpriteSize size)
{
    // Copy 16-color object tiles into appropriate memory, and return a pointer to the data
    uint16_t *gfx = oamAllocateGfx(&oamMain, size, SpriteColorFormat_Bmp);
    if (gfx) dmaCopy(bitmap, gfx, bitmapLen);
    return gfx;
}

void gameInit()
{
    // Allocate bitmap data for the game objects
    gameGfx[0]  = initObjBitmap(triangle_holeBitmap,  triangle_holeBitmapLen,  SpriteSize_32x32);
    gameGfx[1]  = initObjBitmap(circle_holeBitmap,    circle_holeBitmapLen,    SpriteSize_32x32);
    gameGfx[2]  = initObjBitmap(cross_holeBitmap,     cross_holeBitmapLen,     SpriteSize_32x32);
    gameGfx[3]  = initObjBitmap(square_holeBitmap,    square_holeBitmapLen,    SpriteSize_32x32);
    gameGfx[4]  = initObjBitmap(slider_l_holeBitmap,  slider_l_holeBitmapLen,  SpriteSize_32x32);
    gameGfx[5]  = initObjBitmap(slider_r_holeBitmap,  slider_r_holeBitmapLen,  SpriteSize_32x32);
    gameGfx[6]  = initObjBitmap(slider_lh_holeBitmap, slider_lh_holeBitmapLen, SpriteSize_32x32);
    gameGfx[7]  = initObjBitmap(slider_rh_holeBitmap, slider_rh_holeBitmapLen, SpriteSize_32x32);
    gameGfx[8]  = initObjBitmap(triangleBitmap,       triangleBitmapLen,       SpriteSize_32x32);
    gameGfx[9]  = initObjBitmap(circleBitmap,         circleBitmapLen,         SpriteSize_32x32);
    gameGfx[10] = initObjBitmap(crossBitmap,          crossBitmapLen,          SpriteSize_32x32);
    gameGfx[11] = initObjBitmap(squareBitmap,         squareBitmapLen,         SpriteSize_32x32);
    gameGfx[12] = initObjBitmap(slider_lBitmap,       slider_lBitmapLen,       SpriteSize_32x32);
    gameGfx[13] = initObjBitmap(slider_rBitmap,       slider_rBitmapLen,       SpriteSize_32x32);
    gameGfx[14] = initObjBitmap(slider_lhBitmap,      slider_lhBitmapLen,      SpriteSize_32x32);
    gameGfx[15] = initObjBitmap(slider_rhBitmap,      slider_rhBitmapLen,      SpriteSize_32x32);
    gameGfx[16] = initObjBitmap(arrowBitmap,          arrowBitmapLen,          SpriteSize_32x32);
    gameGfx[17] = initObjBitmap(coolBitmap,           coolBitmapLen,           SpriteSize_32x8);
    gameGfx[18] = initObjBitmap(fineBitmap,           fineBitmapLen,           SpriteSize_32x8);
    gameGfx[19] = initObjBitmap(safeBitmap,           safeBitmapLen,           SpriteSize_32x8);
    gameGfx[20] = initObjBitmap(sadBitmap,            sadBitmapLen,            SpriteSize_32x8);
    gameGfx[21] = initObjBitmap(missBitmap,           missBitmapLen,           SpriteSize_32x8);

    // Allocate bitmap data for the combo numbers
    size_t len = combo_numsBitmapLen / 10;
    for (int i = 0; i < 10; i++)
        numGfx[i] = initObjBitmap(&combo_numsBitmap[i * len / sizeof(int)], len, SpriteSize_8x8);
}

static void retryScreen()
{
    stopSong();
    printf("Press start to retry.\n");
    printf("Press select to return to songs.");
    printf("Press dpad to adjust fly time.\n");

    // Show the retry screen
    while (true)
    {
        printf("\x1b[22;0HDefault fly time: %4lu\n", flyTimeDef);

        // Check key input once per frame
        scanKeys();
        uint16_t down = keysDown();
        swiWaitForVBlank();

        // Adjust default fly time with the up and down buttons
        // TODO: figure out how to properly set this
        if ((down & KEY_UP) && flyTimeDef < 3000)
            flyTimeDef += 50;
        else if ((down & KEY_DOWN) && flyTimeDef > 500)
            flyTimeDef -= 50;

        // Open the song selector on select, or reset the chart on start
        if (down & KEY_SELECT)
            songSelector();
        else if (!(down & KEY_START))
            continue;

        // Reset the current chart
        consoleClear();
        notes.clear();
        counter = 1;
        timer = 0;
        flyTime = flyTimeDef * 100;
        finished = false;
        current = 0;
        mask = 0;
        mask2 = 0;
        statTimer = 0;
        combo = 0;
        life = 127;
        return;
    }
}

static void updateChart()
{
    // Execute chart opcodes
    while (counter < chartSize && !finished)
    {
        switch (chart[counter])
        {
            case 0x00: // End
            {
                // Indicate the chart has finished executing
                finished = true;
                return;
            }

            case 0x01: // Time
            {
                // Stop execution until the target time is reached
                if (timer < chart[counter + 1])
                    return;
                break;
            }

            case 0x06: // Target
            {
                static Note note;
                int32_t x = chart[counter + 2] * 256 / 480000 - 16;
                int32_t y = chart[counter + 3] * 192 / 270000 - 16;

                // Set the note type
                if (chart[counter + 1] < 8) // Buttons and holds
                {
                    note.type = chart[counter + 1] & 3;
                }
                else if (chart[counter + 1] == 12 || chart[counter + 1] == 13) // Single slides
                {
                    note.type = chart[counter + 1] - 8;
                }
                else if (chart[counter + 1] == 15) // Left held slide
                {
                    // Mark all held slides with bit 6, and non-initial ones with bit 7
                    uint8_t type = 4 | BIT(6);
                    if (type == (note.type & ~BIT(7)) && x < note.x && y == note.y)
                        type |= BIT(7);
                    note.type = type;
                }
                else if (chart[counter + 1] == 16) // Right held slide
                {
                    // Mark all held slides with bit 6, and non-initial ones with bit 7
                    uint8_t type = 5 | BIT(6);
                    if (type == (note.type & ~BIT(7)) && x > note.x && y == note.y)
                        type |= BIT(7);
                    note.type = type;
                }
                else if (chart[counter + 1] >= 18 && chart[counter + 1] < 22) // Event notes
                {
                    // These normally trigger PV events; treat them as regular notes for now
                    note.type = chart[counter + 1] - 18;
                }
                else
                {
                    break;
                }

                // Get the note angle and travel distance, scaled
                float angle = (float)(int32_t)chart[counter + 4] * PI / 180000;
                float distance = (float)chart[counter + 5] * 0x100 / 270000;

                // Calculate the positional offset and per-frame increment
                note.incX = (int)(sin(angle) *  distance);
                note.incY = (int)(cos(angle) * -distance);
                note.ofsX = note.incX * 60 * 3;
                note.ofsY = note.incY * 60 * 3;
                note.incX *= 100000.0f * 3 / flyTime;
                note.incY *= 100000.0f * 3 / flyTime;

                // Calculate the timing arrow per-frame increment
                note.incArrow = ((float)DEGREES_IN_CIRCLE / 60) * 100000 / flyTime;
                note.ofsArrow = 0;

                // Add a note to the queue
                note.x = x;
                note.y = y;
                note.time = timer + flyTime;
                notes.push_back(note);
                break;
            }

            case 0x19: // Music play
            {
                // Start playing the song
                playSong(songName);
                break;
            }

            case 0x3A: // Target flying time
            {
                // Set the time between a note's creation and when it should be hit
                flyTime = chart[counter + 1] * 100;
                break;
            }
        }

        // Move to the next opcode
        counter += paramCounts[chart[counter]] + 1;
    }
}

void gameLoop()
{
    // Open the file browser on start
    songSelector();

    int32_t statX = 0, statY = 0;
    int32_t statCurX = 0, statCurY = 0;
    uint8_t statType = 0;

    while (true)
    {
        // Update the song and chart
        updateSong();
        updateChart();

        oamClear(&oamMain, 0, 0);
        int sprite = 0;
        int rotscale = 0;

        scanKeys();
        uint16_t held = keysHeld();
        uint16_t down = keysDown();
        uint16_t up   = keysUp();

        if (!notes.empty() && notes[0].time - FRAME_TIME * 12 < timer)
        {
            // Get the keys that need to be pressed for the current notes
            if (!mask)
            {
                statX = statY = 0;
                while (current < notes.size() && notes[current].time == notes[0].time)
                {
                    statX += notes[current].x + 16;
                    statY += notes[current].y + 16;
                    mask |= BIT(notes[current++].type & ~0xC0);
                }
                statX = statX / current - 16;
                statY = statY / current - 28;
            }

            if (mask)
            {
                // Scan key input and track which keys are pressed
                for (int i = 0; i < 6; i++)
                {
                    if ((notes[0].type & BIT(7)) && (held & keys[i]))
                    {
                        if (mask & BIT(i))
                            mask2 |= BIT(i);
                    }
                    else if (!(notes[0].type & BIT(7)) && (down & keys[i]))
                    {
                        if (mask & BIT(i))
                        {
                            mask2 |= BIT(i);
                            continue;
                        }

                        // Miss if a wrong key is pressed
                        statType = 4;
                        statTimer = 60;
                        statCurX = statX;
                        statCurY = statY;
                        combo = 0;
                        life = std::max(0, life - 20);

                        // Clear notes that were missed
                        for (; current > 0; current--)
                            notes.pop_front();
                        mask = mask2 = 0;
                    }
                    else if (up & keys[i])
                    {
                        mask2 &= ~BIT(i);
                    }
                }

                // Abort if failed
                if (!mask)
                    continue;

                if (mask == mask2)
                {
                    if (!(notes[0].type & BIT(7)))
                    {
                        // Check how precisely the note was hit and adjust life
                        // For slides, fine/safe count as cool, and sad counts as fine
                        // TODO: verify timings
                        int32_t offset = abs((int32_t)(notes[0].time - timer));
                        if (offset <= FRAME_TIME * ((notes[0].type < 4) ? 3 : 9))
                        {
                            statType = 0; // Cool
                            combo++;
                            life = std::min(255, life + 2);
                        }
                        else if (offset <= FRAME_TIME * 6 || notes[0].type >= 4)
                        {
                            statType = 1; // Fine
                            combo++;
                            life = std::min(255, life + 1);
                        }
                        else if (offset <= FRAME_TIME * 9)
                        {
                            statType = 2; // Safe
                            combo = 0;
                        }
                        else
                        {
                            statType = 3; // Sad
                            combo = 0;
                            life = std::max(0, life - 10);
                        }

                        // Show the hit status above the note
                        statTimer = 60;
                        statCurX = statX;
                        statCurY = statY;
                    }

                    // Clear the notes that were hit
                    for (; current > 0; current--)
                        notes.pop_front();
                    mask = mask2 = 0;
                }
                else if (notes[0].time + FRAME_TIME * 12 < timer)
                {
                    // Miss if a note wasn't cleared in time
                    if (!(notes[0].type & BIT(7)))
                    {
                        statType = 4;
                        statTimer = 60;
                        statCurX = statX;
                        statCurY = statY;
                        combo = 0;
                        life = std::max(0, life - 20);
                    }

                    // Clear the notes that were missed
                    for (; current > 0; current--)
                        notes.pop_front();
                    mask = mask2 = 0;
                }
            }
        }

        // Draw the hit status while its timer is active
        if (statTimer > 0)
        {
            int32_t x = 32;

            // Draw the combo counter if a combo is ongoing
            if (combo > 1)
            {
                // Adjust status offset to keep centered with combo numbers
                x <<= 1;
                for (uint32_t c = combo; c > 0; c /= 10)
                    x += 7; // 3.5 (half of number width)
                x >>= 1;

                // Draw a number for each decimal place in the combo counter
                for (uint32_t c = combo; c > 0; c /= 10)
                {
                    oamSet(&oamMain, sprite++, statCurX + (x -= 7), statCurY, 0, 1, SpriteSize_8x8,
                        SpriteColorFormat_Bmp, numGfx[c % 10], -1, false, false, false, false, false);
                }
            }

            // Draw the accuracy indicator to the left of the combo counter
            oamSet(&oamMain, sprite++, statCurX + x - 32, statCurY, 0, 1, SpriteSize_32x8,
                SpriteColorFormat_Bmp, gameGfx[17 + statType], -1, false, false, false, false, false);

            statTimer--;
        }

        // Update all queued notes
        for (size_t i = 0; i < notes.size(); i++)
        {
            // Move the note closer to its hole
            int x = notes[i].x + ((notes[i].ofsX -= notes[i].incX) >> 8);
            int y = notes[i].y + ((notes[i].ofsY -= notes[i].incY) >> 8);

            // Draw the note if it's within screen bounds
            if (x > -32 && x < 256 && y > -32 && y < 192)
            {
                uint8_t type = (notes[i].type & ~0xC0) + ((notes[i].type & BIT(7)) ? 10 : 8);
                oamSet(&oamMain, sprite++, x, y, 0, 1, SpriteSize_32x32,
                    SpriteColorFormat_Bmp, gameGfx[type], -1, false, false, false, false, false);
            }
        }

        // Draw holes for all queued notes
        for (size_t i = 0; i < notes.size(); i++)
        {
            if (notes[i].type & BIT(7)) // Held slides
            {
                // Draw a held slide note hole with no timing arrow
                uint8_t type = (notes[i].type & ~0xC0) + 2;
                oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, 1, SpriteSize_32x32,
                    SpriteColorFormat_Bmp, gameGfx[type], -1, false, false, false, false, false);
            }
            else
            {
                // Move the timing arrow further along its rotation
                uint16_t angle = (notes[i].ofsArrow -= notes[i].incArrow);

                // Draw the timing arrow if rotscale objects are still available
                if (rotscale < 32)
                {
                    oamRotateScale(&oamMain, rotscale, angle, intToFixed(1, 8), intToFixed(1, 8));
                    oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, 1, SpriteSize_32x32,
                        SpriteColorFormat_Bmp, gameGfx[16], rotscale++, false, false, false, false, false);
                }

                // Draw a regular note hole
                uint8_t type = (notes[i].type & ~0xC0);
                oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, 1, SpriteSize_32x32,
                    SpriteColorFormat_Bmp, gameGfx[type], -1, false, false, false, false, false);
            }
        }

        // Show the life gauge on the bottom screen
        printf("\x1b[0;0HLife: %3u\n", life);

        // Move to the next frame
        oamUpdate(&oamMain);
        swiWaitForVBlank();
        timer += FRAME_TIME;

        // Check the end conditions
        if (down & KEY_START)
        {
            retryScreen();
        }
        else if (life == 0)
        {
            printf("FAILED...\n");
            retryScreen();
        }
        else if (finished && notes.empty())
        {
            printf("CLEAR!\n");
            retryScreen();
        }
    }
}

void loadChart(std::string &chartName, std::string &songName2, bool retry)
{
    // Load a new chart file into memory
    FILE *chartFile = fopen(chartName.c_str(), "rb");
    fseek(chartFile, 0, SEEK_END);
    chartSize = ftell(chartFile) / 4;
    fseek(chartFile, 0, SEEK_SET);
    if (chart) delete[] chart;
    chart = new uint32_t[chartSize];
    fread(chart, sizeof(uint32_t), chartSize, chartFile);
    fclose(chartFile);

    // Set the chart's song filename
    songName = songName2;

    // Show the retry screen if requested
    if (retry)
        retryScreen();
}
