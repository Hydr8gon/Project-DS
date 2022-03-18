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

#include <maxmod9.h>
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
#include "cool.h"
#include "fine.h"
#include "safe.h"
#include "sad.h"
#include "miss.h"

#include "game.h"
#include "menu.h"

#define PI 3.14159
#define FRAME_TIME 1672

struct Note
{
    uint8_t type;
    int32_t x, y;
    int32_t incX, incY;
    int32_t ofsX, ofsY;
    uint32_t time;
};

static std::deque<Note> notes;

static uint16_t *gameGfx[21];

static mm_stream stream;
static FILE *song = nullptr;

static size_t chartSize = 0;
static uint32_t *chart = nullptr;
static uint32_t flyTimeDef = 1750;

static uint32_t counter = 1;
static uint32_t timer = 0;
static uint32_t flyTime = flyTimeDef * 100;
static uint8_t life = 127;
static uint8_t current = 0;
static uint8_t mask = 0;
static uint8_t mask2 = 0;
static uint8_t statTimer = 0;
static bool finished = false;

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

static mm_word audioCallback(mm_word length, mm_addr dest, mm_stream_formats format)
{
    // Load more PCM samples from file
    fread(dest, sizeof(int16_t), length * 2, song);
    return length;
}

static uint16_t *initObjBitmap(const unsigned int *bitmap, size_t bitmapLen, SpriteSize size)
{
    // Copy 16-color object tiles into appropriate memory, and return a pointer to the data
    uint16_t *gfx = oamAllocateGfx(&oamMain, size, SpriteColorFormat_Bmp);
    if (gfx) dmaCopy(bitmap, gfx, bitmapLen);
    return gfx;
}

void gameInit()
{
    // Prepare bitmap data for the game elements
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
    gameGfx[16] = initObjBitmap(coolBitmap,           coolBitmapLen,           SpriteSize_32x16);
    gameGfx[17] = initObjBitmap(fineBitmap,           fineBitmapLen,           SpriteSize_32x16);
    gameGfx[18] = initObjBitmap(safeBitmap,           safeBitmapLen,           SpriteSize_32x16);
    gameGfx[19] = initObjBitmap(sadBitmap,            sadBitmapLen,            SpriteSize_32x16);
    gameGfx[20] = initObjBitmap(missBitmap,           missBitmapLen,           SpriteSize_32x16);

    // Prepare the audio stream
    stream.sampling_rate = 44100 / 2;
    stream.buffer_length = 1024;
    stream.callback      = audioCallback;
    stream.format        = MM_STREAM_16BIT_STEREO;
    stream.timer         = MM_TIMER0;
    stream.manual        = true;
}

static void retryScreen()
{
    printf("Press start to retry.\n");
    printf("Press select to return to songs.");
    printf("Press dpad to adjust fly time.\n");
    mmStreamClose();

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

        // Open the file browser on select, or reset the chart on start
        if (down & KEY_SELECT)
            fileBrowser();
        else if (!(down & KEY_START))
            continue;

        // Reset the current chart
        consoleClear();
        notes.clear();
        fseek(song, 0, SEEK_SET);
        counter = 1;
        timer = 0;
        flyTime = flyTimeDef * 100;
        life = 127;
        current = 0;
        mask = 0;
        mask2 = 0;
        statTimer = 0;
        finished = false;
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
                else if (chart[counter + 1] == 15 || chart[counter + 1] == 16) // Held slides
                {
                    // Unless a new slide is starting, mark the note with an extra bit
                    uint8_t type = chart[counter + 1] - 11;
                    note.type = type | ((type == (note.type & ~BIT(7)) && y == note.y) << 7);
                }
                else
                {
                    break;
                }

                // Calculate the note increment and offset based on angle and speed
                float angle = (int32_t)chart[counter + 4];
                angle = angle * PI / 180000;
                note.incX = (int)(sin(angle) *  0x100);
                note.incY = (int)(cos(angle) * -0x100);
                note.ofsX = note.incX * 60 * 3;
                note.ofsY = note.incY * 60 * 3;
                note.incX *= 100000.0f * 3 / flyTime;
                note.incY *= 100000.0f * 3 / flyTime;

                // Add a note to the queue
                note.x = chart[counter + 2] * 256 / 480000 - 16;
                note.y = y;
                note.time = timer + flyTime;
                notes.push_back(note);
                break;
            }

            case 0x19: // Music play
            {
                // Start the audio stream if a file is loaded
                if (song)
                    mmStreamOpen(&stream);
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
    fileBrowser();

    int32_t statX = 0, statY = 0;
    int32_t statCurX = 0, statCurY = 0;
    uint8_t statType = 0;

    while (true)
    {
        // Update the song and chart
        mmStreamUpdate();
        updateChart();

        oamClear(&oamMain, 0, 0);
        int sprite = 0;

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
                    mask |= BIT(notes[current++].type & ~BIT(7));
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
                        // TODO: verify timings
                        int32_t offset = abs((int32_t)(notes[0].time - timer));
                        if (offset <= FRAME_TIME * 3)
                        {
                            statType = 0; // Cool
                            life = std::min(255, life + 2);
                        }
                        else if (offset <= FRAME_TIME * 6)
                        {
                            statType = 1; // Fine
                            life = std::min(255, life + 1);
                        }
                        else if (offset <= FRAME_TIME * 9)
                        {
                            statType = 2; // Safe
                        }
                        else
                        {
                            statType = 3; // Sad
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
            oamSet(&oamMain, sprite++, statCurX, statCurY, 0, 1, SpriteSize_32x16,
                SpriteColorFormat_Bmp, gameGfx[16 + statType], 0, false, false, false, false, false);
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
                uint8_t type = (notes[i].type & ~BIT(7)) + ((notes[i].type & BIT(7)) ? 10 : 8);
                oamSet(&oamMain, sprite++, x, y, 0, 1, SpriteSize_32x32,
                    SpriteColorFormat_Bmp, gameGfx[type], 0, false, false, false, false, false);
            }
        }

        // Draw holes for all queued notes
        for (size_t i = 0; i < notes.size(); i++)
        {
            uint8_t type = (notes[i].type & ~BIT(7)) + ((notes[i].type & BIT(7)) ? 2 : 0);
            oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, 1, SpriteSize_32x32,
                SpriteColorFormat_Bmp, gameGfx[type], 0, false, false, false, false, false);
        }

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

void loadChart(FILE *newChart, FILE *newSong, bool retry)
{
    // Load a new chart file into memory
    fseek(newChart, 0, SEEK_END);
    chartSize = ftell(newChart) / 4;
    fseek(newChart, 0, SEEK_SET);
    if (chart) delete[] chart;
    chart = new uint32_t[chartSize];
    fread(chart, sizeof(uint32_t), chartSize, newChart);
    fclose(newChart);

    // Load a new song file
    if (song) fclose(song);
    song = newSong;

    // Show the retry screen if requested
    if (retry)
        retryScreen();
}
