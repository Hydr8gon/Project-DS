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

#include <algorithm>
#include <dirent.h>
#include <vector>

#include <nds.h>

#include "cool.h"
#include "fine.h"
#include "safe.h"
#include "sad.h"
#include "miss.h"

#include "song_list.h"

#include "menu.h"
#include "audio.h"
#include "database.h"
#include "game.h"

static const char a[] = {' ', '>'};

static const std::string ends[] =
{
    "_easy.dsc",
    "_normal.dsc",
    "_hard.dsc",
    "_extreme.dsc",
    "_extreme_1.dsc"
};

static uint16_t *menuGfx[10];

static std::vector<std::string> charts[5];
static size_t difficulty = 1;
static size_t selection = 0;

static int bg = 0;
static uint16_t bgLine = 0;

static void sortSongs()
{
    // Toggle between alphabetical and difficulty level sorting
    static bool mode = false;
    mode = !mode;

    // Sort the songs in each difficulty tab
    for (int i = 0; i < 5; i++)
    {
        sort(charts[i].begin(), charts[i].end(), [i](std::string &a, std::string &b)
        {
            uint8_t diffA = (songData[std::stoi(a)].difficulty >> (i * 5)) & 0x1F;
            uint8_t diffB = (songData[std::stoi(b)].difficulty >> (i * 5)) & 0x1F;

            // Sort alphabetically in alphabetical mode, or as a fallback if difficulties match
            if (mode || diffA == diffB)
            {
                std::string &nameA = songData[std::stoi(a)].name;
                std::string &nameB = songData[std::stoi(b)].name;
                size_t j = 0, k = 0;

                // Define a table of custom character priorities for sorting
                // Letter cases are equal, and numbers come after letters
                static uint8_t prios[0x80] =
                {
                     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x00-0x0F
                     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x10-0x1F
                     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x20-0x2F
                    28, 29, 30, 31, 32, 33, 34, 35, 36, 37,  0,  0,  0,  0,  0,  0, // 0x30-0x3F
                     0,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, // 0x40-0x4F
                    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,  0,  0,  0,  0,  0, // 0x50-0x5F
                     0,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, // 0x60-0x6F
                    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,  0,  0,  0,  0,  0, // 0x70-0x7F
                };

                // Ignore leading characters that aren't letters or numbers in the first string
                while (!prios[(uint8_t)nameA[j]])
                    if (++j == nameA.length())
                        return true;

                // Ignore leading characters that aren't letters or numbers in the second string
                while (!prios[(uint8_t)nameB[k]])
                    if (++k == nameB.length())
                        return false;

                while (true)
                {
                    // Search the strings until a non-matching priority is found
                    if (prios[(uint8_t)nameA[j]] == prios[(uint8_t)nameB[k]])
                    {
                        if (++j == nameA.length())
                            return true;
                        if (++k == nameB.length())
                            return false;
                        continue;
                    }

                    // Sort the songs based on the first non-matching character priority
                    return prios[(uint8_t)nameA[j]] < prios[(uint8_t)nameB[k]];
                }
            }

            // Sort the songs based on difficulty level
            return diffA < diffB;
        });
    }
}

static void bgHBlank()
{
    if (REG_VCOUNT == 0)
    {
        // Reset the background at the start of the frame
        bgTransform[bg]->dy = 0;
        bgShow(bg);
    }
    else if (REG_VCOUNT == 172)
    {
        // Disable the background at the end of the list
        bgHide(bg);
    }
    else if (REG_VCOUNT % (8 * 3) == 3)
    {
        // Adjust the background at every list item based on if it's selected
        bgTransform[bg]->dy = ((REG_VCOUNT == bgLine) ? 4 : (8 * 3 + 4)) << 8;
    }
}

void menuInit()
{
    // Scan chart files (.dsc) and build song ID lists for each difficulty
    if (DIR *dir = opendir("/project-ds/dsc"))
    {
        while (dirent *entry = readdir(dir))
        {
            std::string name = entry->d_name;
            if (name.length() > 6 && name.substr(0, 3) == "pv_" && name[3] >= '0' && name[3] <= '9' &&
                name[4] >= '0' && name[4] <= '9' && name[5] >= '0' && name[5] <= '9')
            {
                for (int i = 0; i < 5; i++)
                {
                    if (name.substr(6) == ends[i])
                    {
                        charts[i].push_back(name.substr(3, 3));
                        break;
                    }
                }
            }
        }

        closedir(dir);
    }

    sortSongs();

    // Allocate bitmap data for the menu objects
    menuGfx[0] = initObjBitmap(&oamSub, coolBitmap, coolBitmapLen, SpriteSize_32x8);
    menuGfx[1] = initObjBitmap(&oamSub, fineBitmap, fineBitmapLen, SpriteSize_32x8);
    menuGfx[2] = initObjBitmap(&oamSub, safeBitmap, safeBitmapLen, SpriteSize_32x8);
    menuGfx[3] = initObjBitmap(&oamSub, sadBitmap,  sadBitmapLen,  SpriteSize_32x8);
    menuGfx[4] = initObjBitmap(&oamSub, missBitmap, missBitmapLen, SpriteSize_32x8);

    // Prepare the song list background
    bg = bgInitSub(3, BgType_Bmp8, BgSize_B8_128x128, 7, 0);
    dmaCopy(song_listBitmap, bgGetGfxPtr(bg), song_listBitmapLen);
    dmaCopy(song_listPal, BG_PALETTE_SUB, 3 * sizeof(uint16_t));
    bgSetRotateScale(bg, 0, 1 << 7, 1 << 8);
    bgUpdate();
    irqSet(IRQ_HBLANK, bgHBlank);
}

void songList()
{
    // Clear any sprites that were set
    oamClear(&oamSub, 0, 0);
    oamUpdate(&oamSub);

    // Ensure there are files present
    if (charts[0].empty() && charts[1].empty() && charts[2].empty() && charts[3].empty() && charts[4].empty())
    {
        consoleClear();
        printf("No DSC files found.\n");
        printf("Place them in '/project-ds/dsc'.\n");

        // Do nothing since there are no files to show
        while (true)
            swiWaitForVBlank();
    }

    irqEnable(IRQ_HBLANK);

    uint8_t frames = 1;

    // Show the file browser
    while (true)
    {
        consoleClear();

        // Calculate the offset to display the files from
        size_t offset = 0;
        if (charts[difficulty].size() > 7)
        {
            if (selection >= charts[difficulty].size() - 4)
                offset = charts[difficulty].size() - 7;
            else if (selection > 2)
                offset = selection - 2;
        }

        // Define accent colors for each difficulty
        static const uint16_t pal[] =
        {
            ARGB16(1,  0, 19, 23), // Easy
            ARGB16(1,  3, 22,  1), // Normal
            ARGB16(1, 26, 18,  0), // Hard
            ARGB16(1, 27,  0,  4), // Extreme
            ARGB16(1, 21,  2, 28)  // Extra Extreme
        };

        // Adjust the background to reflect the current selections
        BG_PALETTE_SUB[2] = pal[difficulty];
        bgLine = (selection - offset) * 8 * 3 + 3;

        // Display a section of songs and their data around the current selection
        for (size_t i = offset; i < offset + std::min(charts[difficulty].size(), 7U); i++)
        {
            SongData &data = songData[std::stoi(charts[difficulty][i])];
            printf("\x1b[30m\x1b[%d;%dH%s", (i - offset) * 3 + 1, (i == selection), data.name.substr(0, 26).c_str());
            printf("\x1b[%d;%dH%4.1f", (i - offset) * 3 + 1, 27 + (i == selection), ((float)((data.difficulty >> (difficulty * 5)) & 0x1F)) / 2);
            printf("\x1b[%d;%dH%07lupt", (i - offset) * 3 + 2, 10 + (i == selection), data.scores[difficulty]);
            printf("\x1b[%d;%dH%6.2f%%", (i - offset) * 3 + 2, 20 + (i == selection), data.clears[difficulty]);
            static const std::string ranks[5] = { "", "(S)", "(G)", "(E)", "(P)", };
            printf("\x1b[%d;%dH%s", (i - offset) * 3 + 2, 28 + (i == selection), ranks[data.ranks[difficulty]].c_str());
        }

        // Display the difficulty tabs
        printf("\x1b[39m\x1b[23;0H%cEasy %cNormal %cHard %cExtrm %cExEx", a[difficulty == 0],
            a[difficulty == 1], a[difficulty == 2], a[difficulty == 3], a[difficulty == 4]);

        uint16_t down = 0;
        uint16_t held = 0;
        keysDown();

        // Wait for button input
        while (!(down & (KEY_A | KEY_Y | KEY_LEFT | KEY_RIGHT)) && !(held & (KEY_UP | KEY_DOWN)))
        {
            scanKeys();
            down = keysDown();
            held = keysHeld();

            // On the first frame inputs are released, start playing a song preview
            if (!(held & (KEY_UP | KEY_DOWN)) && frames > 0)
            {
                frames = 0;
                std::string name = "/project-ds/pcm/pv_" + charts[difficulty][selection] + ".pcm";
                playSong(name);
            }

            updateSong();
            swiWaitForVBlank();
        }

        stopSong();

        if (down & KEY_A)
        {
            // Select the current song and close the menu
            if (!charts[difficulty].empty())
            {
                consoleClear();
                bgHide(bg);
                irqDisable(IRQ_HBLANK);
                break;
            }
        }
        else if (down & KEY_Y)
        {
            // Change how the songs are sorted
            if (frames++ == 0)
            {
                sortSongs();
                selection = 0;
            }
        }
        else if (down & KEY_LEFT)
        {
            // Move the difficulty selection left with wraparound
            if (frames++ == 0)
            {
                selection = 0;
                if (difficulty-- == 0)
                    difficulty = 4;
            }
        }
        else if (down & KEY_RIGHT)
        {
            // Move the difficulty selection right with wraparound
            if (frames++ == 0)
            {
                selection = 0;
                if (++difficulty == 5)
                    difficulty = 0;
            }
        }
        else if (held & KEY_UP)
        {
            // Decrement the current selection with wraparound, continuously after 30 frames
            if ((frames > 30 || frames++ == 0) && selection-- == 0)
                selection = charts[difficulty].size() - 1;
        }
        else if (held & KEY_DOWN)
        {
            // Increment the current selection with wraparound, continuously after 30 frames
            if ((frames > 30 || frames++ == 0) && ++selection == charts[difficulty].size())
                selection = 0;
        }
    }

    // Infer names for all the files that might need to be accessed
    std::string dscName = "/project-ds/dsc/pv_" + charts[difficulty][selection] + ends[difficulty];
    std::string oggName = "/project-ds/ogg/pv_" + charts[difficulty][selection] + ".ogg";
    std::string pcmName = "/project-ds/pcm/pv_" + charts[difficulty][selection] + ".pcm";

    // Try to convert the song if it wasn't found
    FILE *song = fopen(pcmName.c_str(), "rb");
    bool retry = (!song && convertSong(oggName, pcmName));
    fclose(song);

    loadChart(dscName, pcmName, difficulty, retry);
}

void retryMenu()
{
    stopSong();

    size_t selection = 0;
    uint8_t frames = 1;

    while (true)
    {
        // Draw the menu items
        printf("\x1b[10;13H%cRetry", a[selection == 0]);
        printf("\x1b[12;6H%cReturn to Song List", a[selection == 1]);

        uint16_t down = 0;
        uint16_t held = 0;
        keysDown();

        // Wait for button input
        while (!(down & KEY_A) && !(held & (KEY_UP | KEY_DOWN)))
        {
            scanKeys();
            down = keysDown();
            held = keysHeld();
            if (!(held & (KEY_UP | KEY_DOWN)))
                frames = 0;
            swiWaitForVBlank();
        }

        if (down & KEY_A)
        {
            // Handle the selected item
            switch (selection)
            {
                case 1: // Return to Song List
                    songList();
                case 0: // Retry
                    gameReset();
                    break;
            }

            // Return to the game
            consoleClear();
            return;
        }
        else if (held & KEY_UP)
        {
            // Decrement the current selection with wraparound, continuously after 30 frames
            if ((frames > 30 || frames++ == 0) && selection-- == 0)
                selection = 2 - 1;
        }
        else if (held & KEY_DOWN)
        {
            // Increment the current selection with wraparound, continuously after 30 frames
            if ((frames > 30 || frames++ == 0) && ++selection == 2)
                selection = 0;
        }
    }
}

void resultsScreen(Results *results, bool fail)
{
    stopSong();

    // Clear the bottom screen
    oamClear(&oamSub, 0, 0);
    oamUpdate(&oamSub);
    consoleClear();

    static std::string diffs[5] =
    {
        "EASY",
        "NORMAL",
        "HARD",
        "EXTREME",
        "EX EXTREME"
    };

    // Show the difficulty and song name at the top
    SongData &data = songData[std::stoi(charts[difficulty][selection])];
    printf("\x1b[0;0H%s - %s", diffs[difficulty].c_str(), data.name.substr(0, 29 - diffs[difficulty].length()).c_str());

    static uint8_t percents[5][3] =
    {
        { 30, 65, 80 }, // Easy
        { 50, 75, 85 }, // Normal
        { 60, 80, 90 }, // Hard
        { 70, 85, 95 }, // Extreme
        { 70, 85, 95 }  // Extra Extreme
    };

    // Assign a performance rank based on the results, and show it
    uint8_t rank;
    if (results->comboMax == results->total)
    {
        rank = 4;
        printf("\x1b[6;13HCLEAR!");
        printf("\x1b[7;6HPERFECT   %9.02f%%", results->clear);
    }
    else if (fail || results->clear < percents[difficulty][0])
    {
        rank = 0;
        printf("\x1b[6;10HNOT CLEAR...");
        printf("\x1b[7;6HDROPxOUT  %9.02f%%", results->clear);
    }
    else if (results->clear < percents[difficulty][1])
    {
        rank = 1;
        printf("\x1b[6;13HCLEAR!");
        printf("\x1b[7;6HSTANDARD  %9.02f%%", results->clear);
    }
    else if (results->clear < percents[difficulty][2])
    {
        rank = 2;
        printf("\x1b[6;13HCLEAR!");
        printf("\x1b[7;6HGREAT     %9.02f%%", results->clear);
    }
    else
    {
        rank = 3;
        printf("\x1b[6;13HCLEAR!");
        printf("\x1b[7;6HEXCELLENT %9.02f%%", results->clear);
    }

    // Use sprites for each of the different note accuracies
    oamSet(&oamSub, 0, 6 * 8 - 2,  9 * 8, 0, 1, SpriteSize_32x8, SpriteColorFormat_Bmp, menuGfx[0], -1, false, false, false, false, false);
    oamSet(&oamSub, 1, 6 * 8 - 3, 10 * 8, 0, 1, SpriteSize_32x8, SpriteColorFormat_Bmp, menuGfx[1], -1, false, false, false, false, false);
    oamSet(&oamSub, 2, 6 * 8 - 2, 11 * 8, 0, 1, SpriteSize_32x8, SpriteColorFormat_Bmp, menuGfx[2], -1, false, false, false, false, false);
    oamSet(&oamSub, 3, 6 * 8 - 6, 12 * 8, 0, 1, SpriteSize_32x8, SpriteColorFormat_Bmp, menuGfx[3], -1, false, false, false, false, false);
    oamSet(&oamSub, 4, 6 * 8 - 3, 13 * 8, 0, 1, SpriteSize_32x8, SpriteColorFormat_Bmp, menuGfx[4], -1, false, false, false, false, false);
    oamUpdate(&oamSub);

    // Show statistics from the results
    printf("\x1b[9;6H%12lu/%6.02f%%",  results->cools,  100.0f * results->cools  / results->total);
    printf("\x1b[10;6H%12lu/%6.02f%%", results->fines,  100.0f * results->fines  / results->total);
    printf("\x1b[11;6H%12lu/%6.02f%%", results->safes,  100.0f * results->safes  / results->total);
    printf("\x1b[12;6H%12lu/%6.02f%%", results->sads,   100.0f * results->sads   / results->total);
    printf("\x1b[13;6H%12lu/%6.02f%%", results->misses, 100.0f * results->misses / results->total);
    printf("\x1b[14;6HCOMBO %13lu", results->comboMax);
    printf("\x1b[15;6HHOLD  %13lu", results->scoreHold);
    printf("\x1b[16;6HSLIDE %13lu", results->scoreSlide);

    // Show the total score at the bottom
    uint32_t score = results->scoreBase + results->scoreHold + results->scoreSlide;
    printf("\x1b[18;6HSCORE %13lu", score);

    // Update the saved scores if any records were broken
    bool update = false;
    if (score > data.scores[difficulty])
        update = true, data.scores[difficulty] = score;
    if (results->clear > data.clears[difficulty])
        update = true, data.clears[difficulty] = results->clear;
    if (rank > data.ranks[difficulty])
        update = true, data.ranks[difficulty] = rank;
    if (update)
        writeScores();

    uint16_t down = 0;
    keysDown();

    // Wait for the A button to be pressed
    while (!(down & KEY_A))
    {
        scanKeys();
        down = keysDown();
        swiWaitForVBlank();
    }

    // When A is pressed, clear the screen and show the retry menu
    oamClear(&oamSub, 0, 0);
    oamUpdate(&oamSub);
    consoleClear();
    retryMenu();
}
