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

#include "life_empty.h"
#include "life_full.h"

#include "game.h"
#include "audio.h"
#include "database.h"
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

static uint16_t *numGfx[10];
static uint16_t *mainGfx[23];
static uint16_t *subGfx[2];

static size_t chartSize = 0;
static uint32_t *chart = nullptr;
static std::string songName;

static uint32_t counter = 1;
static uint32_t timer = 0;
static uint32_t flyTime = 100000;
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

static uint32_t scoreBase = 0;
static uint32_t scoreHold = 0;
static uint32_t scoreSlide = 0;
static uint32_t scoreRef = 0;

static uint8_t clearRate = 30;
static uint8_t holdDivide = 1;

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

static uint16_t *initObjBitmap(OamState *oam, const unsigned int *bitmap, size_t bitmapLen, SpriteSize size)
{
    // Copy 16-color object tiles into appropriate memory, and return a pointer to the data
    uint16_t *gfx = oamAllocateGfx(oam, size, SpriteColorFormat_Bmp);
    if (gfx) dmaCopy(bitmap, gfx, bitmapLen);
    return gfx;
}

void gameInit()
{
    // Allocate bitmap data for the combo numbers
    size_t len = combo_numsBitmapLen / 10;
    for (int i = 0; i < 10; i++)
        numGfx[i] = initObjBitmap(&oamMain, &combo_numsBitmap[i * len / sizeof(int)], len, SpriteSize_8x8);

    // Allocate bitmap data for the main screen objects
    mainGfx[0]  = initObjBitmap(&oamMain, triangle_holeBitmap,  triangle_holeBitmapLen,  SpriteSize_32x32);
    mainGfx[1]  = initObjBitmap(&oamMain, circle_holeBitmap,    circle_holeBitmapLen,    SpriteSize_32x32);
    mainGfx[2]  = initObjBitmap(&oamMain, cross_holeBitmap,     cross_holeBitmapLen,     SpriteSize_32x32);
    mainGfx[3]  = initObjBitmap(&oamMain, square_holeBitmap,    square_holeBitmapLen,    SpriteSize_32x32);
    mainGfx[4]  = initObjBitmap(&oamMain, slider_l_holeBitmap,  slider_l_holeBitmapLen,  SpriteSize_32x32);
    mainGfx[5]  = initObjBitmap(&oamMain, slider_r_holeBitmap,  slider_r_holeBitmapLen,  SpriteSize_32x32);
    mainGfx[6]  = initObjBitmap(&oamMain, slider_lh_holeBitmap, slider_lh_holeBitmapLen, SpriteSize_32x32);
    mainGfx[7]  = initObjBitmap(&oamMain, slider_rh_holeBitmap, slider_rh_holeBitmapLen, SpriteSize_32x32);
    mainGfx[8]  = initObjBitmap(&oamMain, triangleBitmap,       triangleBitmapLen,       SpriteSize_32x32);
    mainGfx[9]  = initObjBitmap(&oamMain, circleBitmap,         circleBitmapLen,         SpriteSize_32x32);
    mainGfx[10] = initObjBitmap(&oamMain, crossBitmap,          crossBitmapLen,          SpriteSize_32x32);
    mainGfx[11] = initObjBitmap(&oamMain, squareBitmap,         squareBitmapLen,         SpriteSize_32x32);
    mainGfx[12] = initObjBitmap(&oamMain, slider_lBitmap,       slider_lBitmapLen,       SpriteSize_32x32);
    mainGfx[13] = initObjBitmap(&oamMain, slider_rBitmap,       slider_rBitmapLen,       SpriteSize_32x32);
    mainGfx[14] = initObjBitmap(&oamMain, slider_lhBitmap,      slider_lhBitmapLen,      SpriteSize_32x32);
    mainGfx[15] = initObjBitmap(&oamMain, slider_rhBitmap,      slider_rhBitmapLen,      SpriteSize_32x32);
    mainGfx[16] = initObjBitmap(&oamMain, arrowBitmap,          arrowBitmapLen,          SpriteSize_32x32);
    mainGfx[17] = initObjBitmap(&oamMain, holdBitmap,           holdBitmapLen,           SpriteSize_32x16);
    mainGfx[18] = initObjBitmap(&oamMain, coolBitmap,           coolBitmapLen,           SpriteSize_32x8);
    mainGfx[19] = initObjBitmap(&oamMain, fineBitmap,           fineBitmapLen,           SpriteSize_32x8);
    mainGfx[20] = initObjBitmap(&oamMain, safeBitmap,           safeBitmapLen,           SpriteSize_32x8);
    mainGfx[21] = initObjBitmap(&oamMain, sadBitmap,            sadBitmapLen,            SpriteSize_32x8);
    mainGfx[22] = initObjBitmap(&oamMain, missBitmap,           missBitmapLen,           SpriteSize_32x8);

    // Allocate bitmap data for the sub screen objects
    subGfx[0]  = initObjBitmap(&oamSub, life_emptyBitmap, life_emptyBitmapLen, SpriteSize_32x8);
    subGfx[1]  = initObjBitmap(&oamSub, life_fullBitmap,  life_fullBitmapLen,  SpriteSize_32x8);
}

static void clearLyrics()
{
    // Clear lyrics with empty space so new ones can be drawn
    for (int i = 10; i <= 12; i++)
        printf("\x1b[%d;0H                                ", i);
}

static uint32_t calcRefScore()
{
    uint32_t score = 0;
    uint32_t count = 1;
    bool finish = false;
    bool multi = false;

    // Scan the whole chart and add up the reference score
    while (count < chartSize && !finish)
    {
        switch (chart[count])
        {
            case 0x00: // End
            {
                finish = true;
                break;
            }

            case 0x01: // Time
            {
                multi = false;
                break;
            }

            case 0x06: // Target
            {
                static Note note;
                int32_t x = chart[count + 2] * 256 / 480000 - 16;
                int32_t y = chart[count + 3] * 192 / 270000 - 16;

                // Set the note type
                if (chart[count + 1] < 8 || // Normal and held buttons
                    chart[count + 1] == 12 || chart[count + 1] == 13 || // Single slides
                    (chart[count + 1] >= 18 && chart[count + 1] < 22)) // Event notes
                {
                    // The note type doesn't matter for non-special cases, so ignore it
                    note.type = 0;
                }
                else if (chart[count + 1] == 15) // Left held slide
                {
                    // Mark all held slides with bit 6, and non-initial ones with bit 7
                    uint8_t type = 4 | BIT(6);
                    if (type == (note.type & ~BIT(7)) && x < note.x && y == note.y)
                        type |= BIT(7);
                    note.type = type;
                }
                else if (chart[count + 1] == 16) // Right held slide
                {
                    // Mark all held slides with bit 6, and non-initial ones with bit 7
                    uint8_t type = 5 | BIT(6);
                    if (type == (note.type & ~BIT(7)) && x > note.x && y == note.y)
                        type |= BIT(7);
                    note.type = type;
                }
                else
                {
                    break;
                }

                // Remember the current note's coordinates
                note.x = x;
                note.y = y;

                // Increase the reference score
                // Combo bonus doesn't apply to non-initial multi-notes and slides
                score += 500 + ((multi || (note.type & BIT(7))) ? 0 : 250);
                multi = true;
                break;
            }
        }

        // Move to the next opcode
        count += paramCounts[chart[count]] + 1;
    }

    // Return the reference score, adjusted for combos below 50
    return score - 7250;
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

            case 0x18: // Lyric
            {
                clearLyrics();

                // Get a lyric from the song database and display it on the bottom screen
                std::vector<std::string> &lyrics = songData[std::stoi(songName.substr(19, 3))].lyrics;
                if (chart[counter + 1] > 0 && chart[counter + 1] < lyrics.size())
                {
                    std::string &lyric = lyrics[chart[counter + 1] - 1];
                    if (lyric.length() > 32)
                    {
                        // Split the lyric into two lines and draw them, centered
                        size_t split = lyric.substr(0, 32).find_last_of(" ");
                        size_t offset = (32 - split) / 2;
                        printf("\x1b[10;%uH%s", offset, lyric.substr(0, split).c_str());
                        offset = (32 - (lyric.length() - (split + 1))) / 2;
                        printf("\x1b[12;%uH%s", offset, lyric.substr(split + 1).c_str());
                    }
                    else
                    {
                        // Draw the lyric on one line, centered
                        size_t offset = (32 - lyric.length()) / 2;
                        printf("\x1b[11;%uH%s", offset, lyric.c_str());
                    }
                }

                break;
            }

            case 0x19: // Music play
            {
                // Start playing the song
                playSong(songName);
                break;
            }

            case 0x1C: // Bar time set
            {
                // Set the flying time using beats per minute and beats per bar
                flyTime = (60.0f / chart[counter + 1]) * (chart[counter + 2] + 1) * 100000;
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
    // Open the song list on start
    songList();

    int32_t statX = 0, statY = 0;
    int32_t statCurX = 0, statCurY = 0;
    uint8_t statType = 0;

    while (true)
    {
        // Update the song and chart
        updateSong();
        updateChart();

        oamClear(&oamMain, 0, 0);
        oamClear(&oamSub, 0, 0);
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
                            scoreBase += 250;
                        }
                        else if (offset <= FRAME_TIME * 6 || (notes[0].type & 0xE0)) // Wrong (black)
                        {
                            life = std::max(0, life - 6);
                            scoreBase += 150;
                        }
                        else if (offset <= FRAME_TIME * 9) // Wrong (green)
                        {
                            life = std::max(0, life - 9);
                            scoreBase += 50;
                        }
                        else // Wrong (blue)
                        {
                            life = std::max(0, life - 15);
                            scoreBase += 30;
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
                        scoreBase += 500;
                        scoreSlide += (++slideCount) * 10;

                        // Detect the end of a held slide
                        if (notes.size() == current || !(notes[current].type & BIT(7)))
                        {
                            // Add a 1000-point bonus if the slide was never broken
                            if (!slideBroken)
                                scoreSlide += 1000;

                            // Reset the slide stats for the next one
                            slideCount = 0;
                            slideBroken = false;
                        }

                        // Add a 10-point bonus at full health
                        if (life == 255)
                            scoreBase += 10;
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
                            scoreBase += 500 * current;

                            // Add a 10-point bonus at full health
                            if (life == 255)
                                scoreBase += 10;
                        }
                        else if (offset <= FRAME_TIME * 6 || (notes[0].type & 0xE0)) // Fine
                        {
                            statType = 1;
                            combo++;
                            life = std::min(255, life + 1);
                            scoreBase += 300 * current;
                        }
                        else if (offset <= FRAME_TIME * 9) // Safe
                        {
                            statType = 2;
                            combo = 0;
                            scoreBase += 100 * current;
                        }
                        else // Sad
                        {
                            statType = 3;
                            combo = 0;
                            life = std::max(0, life - 10);
                            scoreBase += 50 * current;
                        }

                        // Show the hit status above the note
                        statTimer = 60;
                        statCurX = statX;
                        statCurY = statY;

                        // Add a score bonus based on current combo
                        if (combo >= 50)
                            scoreBase += 250;
                        else if (combo >= 40)
                            scoreBase += 200;
                        else if (combo >= 30)
                            scoreBase += 150;
                        else if (combo >= 20)
                            scoreBase += 100;
                        else if (combo >= 10)
                            scoreBase += 50;

                        for (int i = 0; i < current; i++)
                        {
                            // Cancel note holds if a held note is hit again
                            if (holdNotes & BIT(notes[i].type & 0xF))
                            {
                                holdNotes = 0;
                                holdStart = 0;
                                holdTime = 0;
                                holdScore = 0;
                            }

                            // Track note holds and reset the max hold time when a new one starts
                            if (notes[i].type & BIT(4))
                            {
                                holdNotes |= BIT(notes[i].type & 0xF);
                                holdTime = 0;
                            }
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
                        scoreHold += 10;
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
                    scoreHold += holdScore;
            }

            if (++holdTime == 5 * 60) // 5 seconds
            {
                // Add a 1500-point bonus per note hold if the max hold time is reached
                for (int i = 0; i < 4; i++)
                {
                    if (holdNotes & BIT(i))
                        scoreHold += 1500;
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
                SpriteColorFormat_Bmp, mainGfx[18 + statType], -1, false, false, false, false, false);

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
                    SpriteColorFormat_Bmp, mainGfx[type], -1, false, false, false, false, false);
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
                    SpriteColorFormat_Bmp, mainGfx[type], -1, false, false, false, false, false);
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
                        SpriteColorFormat_Bmp, mainGfx[16], rotscale++, false, false, false, false, false);
                }

                // Draw the hold indicator if the note is held
                if (notes[i].type & BIT(4))
                {
                    oamSet(&oamMain, sprite++, notes[i].x, notes[i].y + 20, 0, 1, SpriteSize_32x16,
                        SpriteColorFormat_Bmp, mainGfx[17], -1, false, false, false, false, false);
                }

                // Draw a regular note hole
                uint8_t type = (notes[i].type & 0xF);
                oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, 1, SpriteSize_32x32,
                    SpriteColorFormat_Bmp, mainGfx[type], -1, false, false, false, false, false);
            }
        }

        // Draw the empty life gauge, split into 2 rotscaled sprites
        oamRotateScale(&oamSub, 0, 0, 1 << 7, 1 << 8);
        oamSet(&oamSub, 0, 4 * 8, -4, 1, 1, SpriteSize_32x8,
            SpriteColorFormat_Bmp, subGfx[0], 0, true, false, false, false, false);
        oamSet(&oamSub, 1, 4 * 8 + 64, -4, 1, 1, SpriteSize_32x8,
            SpriteColorFormat_Bmp, subGfx[0], 0, true, false, false, false, false);

        if (life > 127)
        {
            // Draw one full life gauge sprite, and scale a second one based on upper life
            oamRotateScale(&oamSub, 1, 0, (1 << 14) / (life - 127), 1 << 8);
            oamSet(&oamSub, 2, 4 * 8, -4, 0, 1, SpriteSize_32x8,
                SpriteColorFormat_Bmp, subGfx[1], 0, true, false, false, false, false);
            oamSet(&oamSub, 3, 4 * 8 + 32 + (life - 127) / 4, -4, 0, 1, SpriteSize_32x8,
                SpriteColorFormat_Bmp, subGfx[1], 1, true, false, false, false, false);
        }
        else if (life > 0)
        {
            // Draw one life gauge sprite, scaled based on lower life
            oamRotateScale(&oamSub, 1, 0, (1 << 14) / (life + 1), 1 << 8);
            oamSet(&oamSub, 2, 4 * 8 - 32 + (life + 1) / 4, -4, 0, 1, SpriteSize_32x8,
                SpriteColorFormat_Bmp, subGfx[1], 1, true, false, false, false, false);
        }

        // Calculate the current clear percentage, with up to 5% bonus from holds
        uint32_t holdBonus = std::min(scoreRef / 20, (scoreHold * 4) / holdDivide);
        float clear = (100.0f * (scoreBase + holdBonus)) / scoreRef;

        // Draw the sub screen text-based UI elements
        printf("\x1b[0;0HLIFE");
        printf("\x1b[0;25H%07lu", scoreBase + scoreHold + scoreSlide);
        printf("\x1b[23;0H%.02f%%", clear);

        // Move to the next frame
        oamUpdate(&oamMain);
        oamUpdate(&oamSub);
        swiWaitForVBlank();
        timer += FRAME_TIME;

        // Check the stop conditions
        if (down & KEY_START)
        {
            clearLyrics();
            retryMenu();
        }
        else if (life == 0)
        {
            printf("\x1b[6;12HFAILED...\n");
            clearLyrics();
            retryMenu();
        }
        else if (finished && notes.empty())
        {
            if (clear < clearRate)
                printf("\x1b[6;12HFAILED...\n");
            else
                printf("\x1b[6;13HCLEAR!\n");
            clearLyrics();
            retryMenu();
        }
    }
}

void gameReset()
{
    // Reset the current chart
    notes.clear();
    counter = 1;
    timer = 0;
    flyTime = 100000;
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
    scoreBase = 0;
    scoreHold = 0;
    scoreSlide = 0;
}

void loadChart(std::string &chartName, std::string &songName2, size_t difficulty, bool retry)
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

    // Set the difficulty-based modifiers
    switch (difficulty)
    {
        case 0: // Easy
            clearRate = 30;
            holdDivide = 1; // 4 / 1 = 4
            break;

        case 1: // Normal
            clearRate = 50;
            holdDivide = 2; // 4 / 2 = 2
            break;

        case 2: // Hard
            clearRate = 60;
            holdDivide = 8; // 4 / 8 = 0.5
            break;

        default: // Extreme
            clearRate = 70;
            holdDivide = 20; // 4 / 20 = 0.2
            break;
    }

    // Calculate the reference score for clear percentage
    scoreRef = calcRefScore();

    // Show the retry menu if requested
    if (retry)
        retryMenu();
}
