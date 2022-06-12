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

static std::vector<std::string> charts[5];

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

    static size_t difficulty = 1;
    static size_t selection = 0;
    uint8_t frames = 1;

    // Show the file browser
    while (true)
    {
        // Calculate the offset to display the files from
        size_t offset = 0;
        if (charts[difficulty].size() > 21)
        {
            if (selection >= charts[difficulty].size() - 10)
                offset = charts[difficulty].size() - 21;
            else if (selection > 10)
                offset = selection - 10;
        }

        consoleClear();

        // Display a section of songs around the current selection
        for (size_t i = offset; i < offset + std::min(charts[difficulty].size(), 21U); i++)
        {
            SongData &data = songData[std::stoi(charts[difficulty][i])];
            printf("\x1b[%d;0H%c%s\n", i - offset, a[i == selection], data.name.substr(0, 26).c_str());
            printf("\x1b[%d;28H%4.1f\n", i - offset, ((float)((data.difficulty >> (difficulty * 5)) & 0x1F)) / 2);
        }

        // Display the difficulty tabs
        printf("\x1b[22;0H%cEasy %cNormal %cHard %cExtrm %cExEx", a[difficulty == 0],
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
            // Select the current file and proceed to load it
            if (!charts[difficulty].empty())
            {
                consoleClear();
                break;
            }
        }
        else if (down & KEY_Y)
        {
            // Change how the songs are sorted
            sortSongs();
            selection = 0;
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
