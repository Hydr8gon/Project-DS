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
#include "game.h"

static const char a[] = {' ', '>'};

void songList()
{
    std::vector<std::string> charts[5];
    DIR *dir = opendir("/project-ds/dsc");
    dirent *entry;

    static const std::string ends[] =
    {
        "_easy.dsc",
        "_normal.dsc",
        "_hard.dsc",
        "_extreme.dsc",
        "_extreme_1.dsc",
    };

    // Sort DSC files based on their difficulty suffix
    while ((entry = readdir(dir)))
    {
        std::string name = entry->d_name;
        for (int i = 0; i < 5; i++)
        {
            if (name.length() > ends[i].length() && name.find(ends[i], name.length() - ends[i].length()) != std::string::npos)
            {
                charts[i].push_back(name.substr(0, name.length() - ends[i].length()));
                break;
            }
        }
    }

    closedir(dir);
    for (int i = 0; i < 5; i++)
        sort(charts[i].begin(), charts[i].end());

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

        // Display a section of files around the current selection
        for (size_t i = offset; i < offset + std::min(charts[difficulty].size(), 21U); i++)
            printf("\x1b[%d;0H%c%s\n", i - offset, a[i == selection], charts[difficulty][i].c_str());

        // Display the difficulty tabs
        printf("\x1b[22;0H%cEasy %cNormal %cHard %cExtrm %cExEx", a[difficulty == 0],
            a[difficulty == 1], a[difficulty == 2], a[difficulty == 3], a[difficulty == 4]);

        uint16_t held = 0;
        uint16_t inputs = KEY_A | KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT;

        // Wait for button input
        while (!(held & inputs))
        {
            scanKeys();
            held = keysHeld();

            // On the first frame inputs are released, start playing a song preview
            if (!(held & inputs) && frames > 0)
            {
                frames = 0;
                std::string name = "/project-ds/pcm/" + charts[difficulty][selection] + ".pcm";
                playSong(name);
            }

            updateSong();
            swiWaitForVBlank();
        }

        stopSong();

        if (held & KEY_A)
        {
            // Select the current file and proceed to load it
            if (!charts[difficulty].empty())
            {
                consoleClear();
                break;
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
        else if (held & KEY_LEFT)
        {
            // Move the difficulty selection left with wraparound
            if (frames++ == 0)
            {
                selection = 0;
                if (difficulty-- == 0)
                    difficulty = 4;
            }
        }
        else if (held & KEY_RIGHT)
        {
            // Move the difficulty selection right with wraparound
            if (frames++ == 0)
            {
                selection = 0;
                if (++difficulty == 5)
                    difficulty = 0;
            }
        }
    }

    // Infer names for all the files that might need to be accessed
    std::string dscName = "/project-ds/dsc/" + charts[difficulty][selection] + ends[difficulty];
    std::string oggName = "/project-ds/ogg/" + charts[difficulty][selection] + ".ogg";
    std::string pcmName = "/project-ds/pcm/" + charts[difficulty][selection] + ".pcm";

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

        uint16_t held = 0;
        uint16_t inputs = KEY_A | KEY_UP | KEY_DOWN;

        // Wait for button input
        while (!(held & inputs))
        {
            scanKeys();
            held = keysHeld();
            if (!(held & inputs))
                frames = 0;
            swiWaitForVBlank();
        }

        if (held & KEY_A)
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
