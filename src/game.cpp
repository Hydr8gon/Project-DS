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
#include "hold.h"
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

static uint16_t *gameGfx[23];
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

static uint8_t holdNotes = 0;
static uint8_t holdStart = 0;
static uint16_t holdTime = 0;
static uint16_t holdScore = 0;

static uint32_t slideCount = 0;
static bool slideBroken = false;

static uint32_t combo = 0;
static uint8_t life = 127;
static uint32_t score = 0;

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
    gameGfx[17] = initObjBitmap(holdBitmap,           holdBitmapLen,           SpriteSize_32x16);
    gameGfx[18] = initObjBitmap(coolBitmap,           coolBitmapLen,           SpriteSize_32x8);
    gameGfx[19] = initObjBitmap(fineBitmap,           fineBitmapLen,           SpriteSize_32x8);
    gameGfx[20] = initObjBitmap(safeBitmap,           safeBitmapLen,           SpriteSize_32x8);
    gameGfx[21] = initObjBitmap(sadBitmap,            sadBitmapLen,            SpriteSize_32x8);
    gameGfx[22] = initObjBitmap(missBitmap,           missBitmapLen,           SpriteSize_32x8);

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
        holdNotes = 0;
        holdStart = 0;
        holdTime = 0;
        holdScore = 0;
        slideCount = 0;
        slideBroken = false;
        combo = 0;
        life = 127;
        score = 0;
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
                if (chart[counter + 1] < 4) // Normal buttons
                {
                    note.type = chart[counter + 1];
                }
                else if (chart[counter + 1] < 8) // Held buttons
                {
                    // Mark held buttons with bit 4
                    note.type = (chart[counter + 1] & 3) | BIT(4);
                }
                else if (chart[counter + 1] == 12 || chart[counter + 1] == 13) // Single slides
                {
                    // Mark single slides with bit 5
                    note.type = (chart[counter + 1] - 8) | BIT(5);
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
                    mask |= BIT(notes[current++].type & 0xF);
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

                        // Check how precisely the note was hit and adjust life and score
                        // For slides, fine/safe count as cool, and sad counts as fine
                        // TODO: verify timings, add unique graphics
                        int32_t offset = abs((int32_t)(notes[0].time - timer));
                        if (offset <= FRAME_TIME * ((notes[0].type & 0xE0) ? 9 : 3)) // Wrong (red)
                        {
                            life = std::max(0, life - 3);
                            score += 250;
                        }
                        else if (offset <= FRAME_TIME * 6 || (notes[0].type & 0xE0)) // Wrong (black)
                        {
                            life = std::max(0, life - 6);
                            score += 150;
                        }
                        else if (offset <= FRAME_TIME * 9) // Wrong (green)
                        {
                            life = std::max(0, life - 9);
                            score += 50;
                        }
                        else // Wrong (blue)
                        {
                            life = std::max(0, life - 15);
                            score += 30;
                        }

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
                    if (notes[0].type & BIT(7))
                    {
                        // Adjust score for non-initial held slides, which are always cool
                        // A score bonus is added based on the current "combo" of these notes
                        // TODO: draw the score bonus UI
                        score += 500 + (++slideCount) * 10;

                        // Detect the end of a held slide
                        if (notes.size() == current || !(notes[current].type & BIT(7)))
                        {
                            // Add a 1000-point bonus if the slide was never broken
                            if (!slideBroken)
                                score += 1000;

                            // Reset the slide stats for the next one
                            slideCount = 0;
                            slideBroken = false;
                        }

                        // Add a 10-point bonus at full health
                        if (life == 255)
                            score += 10;
                    }
                    else
                    {
                        // Check how precisely the note was hit and adjust life and score
                        // For slides, fine/safe count as cool, and sad counts as fine
                        // TODO: verify timings
                        int32_t offset = abs((int32_t)(notes[0].time - timer));
                        if (offset <= FRAME_TIME * ((notes[0].type & 0xE0) ? 9 : 3)) // Cool
                        {
                            statType = 0;
                            combo++;
                            life = std::min(255, life + 2);
                            score += 500 * current;

                            // Add a 10-point bonus at full health
                            if (life == 255)
                                score += 10;
                        }
                        else if (offset <= FRAME_TIME * 6 || (notes[0].type & 0xE0)) // Fine
                        {
                            statType = 1;
                            combo++;
                            life = std::min(255, life + 1);
                            score += 300 * current;
                        }
                        else if (offset <= FRAME_TIME * 9) // Safe
                        {
                            statType = 2;
                            combo = 0;
                            score += 100 * current;
                        }
                        else // Sad
                        {
                            statType = 3;
                            combo = 0;
                            life = std::max(0, life - 10);
                            score += 50 * current;
                        }

                        // Show the hit status above the note
                        statTimer = 60;
                        statCurX = statX;
                        statCurY = statY;

                        // Add a score bonus based on current combo
                        if (combo >= 50)
                            score += 250;
                        else if (combo >= 40)
                            score += 200;
                        else if (combo >= 30)
                            score += 150;
                        else if (combo >= 20)
                            score += 100;
                        else if (combo >= 10)
                            score += 50;

                        if (notes[0].type & BIT(4))
                        {
                            // If the note was held, start tracking it for score bonuses
                            for (int i = 0; i < current; i++)
                                holdNotes |= BIT(notes[i].type & 0xF);

                            // Reset the max hold time when a new hold starts
                            holdTime = 0;
                        }
                    }

                    // Clear the notes that were hit
                    for (; current > 0; current--)
                        notes.pop_front();
                    mask = mask2 = 0;
                }
                else if (notes[0].time + FRAME_TIME * 12 < timer)
                {
                    if (notes[0].type & BIT(7))
                    {
                        // Break the slide combo if a held slide wasn't cleared in time
                        slideCount = 0;
                        slideBroken = true;
                    }
                    else
                    {
                        // Miss if a note wasn't cleared in time
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

        // Cancel note holds if one was released
        for (int i = 0; i < 4; i++)
        {
            if ((holdNotes & BIT(i)) && !(held & keys[i]))
            {
                holdNotes = 0;
                holdStart = 0;
                holdTime = 0;
                holdScore = 0;
            }
        }

        // Add score bonuses for note holds
        // TODO: draw the score bonus UI
        if (holdNotes)
        {
            if (holdStart == 12)
            {
                // Add a 10-point bonus per note hold every frame
                for (int i = 0; i < 4; i++)
                {
                    if (holdNotes & BIT(i))
                        score += 10;
                }
            }
            else
            {
                // Queue a 10-point bonus per note hold every frame
                for (int i = 0; i < 4; i++)
                {
                    if (holdNotes & BIT(i))
                        holdScore += 10;
                }

                // After 12 frames, commit to the hold and add the queued bonus
                if (++holdStart == 12)
                    score += holdScore;
            }

            if (++holdTime == 5 * 60) // 5 seconds
            {
                // Add a 1500-point bonus per note hold if the max hold time is reached
                for (int i = 0; i < 4; i++)
                {
                    if (holdNotes & BIT(i))
                        score += 1500;
                }

                // Cancel note holds after the max hold time is reached
                holdNotes = 0;
                holdStart = 0;
                holdTime = 0;
                holdScore = 0;
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
                SpriteColorFormat_Bmp, gameGfx[18 + statType], -1, false, false, false, false, false);

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
                uint8_t type = (notes[i].type & 0xF) + ((notes[i].type & BIT(7)) ? 10 : 8);
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
                uint8_t type = (notes[i].type & 0xF) + 2;
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

                // Draw the hold indicator if the note is held
                if (notes[i].type & BIT(4))
                {
                    oamSet(&oamMain, sprite++, notes[i].x, notes[i].y + 20, 0, 1, SpriteSize_32x16,
                        SpriteColorFormat_Bmp, gameGfx[17], -1, false, false, false, false, false);
                }

                // Draw a regular note hole
                uint8_t type = (notes[i].type & 0xF);
                oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, 1, SpriteSize_32x32,
                    SpriteColorFormat_Bmp, gameGfx[type], -1, false, false, false, false, false);
            }
        }

        // Show the life gauge and score on the bottom screen
        printf("\x1b[0;0HLife: %03u", life);
        printf("\x1b[0;18HScore: %07lu", score);

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
