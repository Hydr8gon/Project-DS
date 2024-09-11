/*
    Copyright 2022-2024 Hydr8gon

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

#include <nds.h>

#include "database.h"

static const std::string diffs[] =
{
    "easy.0.",
    "normal.0.",
    "hard.0.",
    "extreme.0.",
    "extreme.1."
};

SongData songData[1000];

static void formatString(std::string &string)
{
    for (auto c = string.begin(); c != string.end();)
    {
        // Replace any unsupported characters with a space
        if (*c >= 128)
            *c = ' ';

        // Remove consecutive spaces
        if (*c == ' ' && (c == string.begin() || *(c - 1) == ' '))
            c = string.erase(c);
        else
            c++;
    }

    // Remove the terminating newline character
    string.erase(string.end() - 1);
}

void databaseInit()
{
    // Assign default song names as fallbacks
    for (size_t i = 0; i < 1000; i++)
    {
        std::string num = std::to_string(i);
        songData[i].name = "pv_" + std::string(3 - num.length(), '0') + num;
    }

    // Scan database files (.txt) for English song information
    if (DIR *dir = opendir("/project-ds/db"))
    {
        while (dirent *entry = readdir(dir))
        {
            std::string name = (std::string)"/project-ds/db/" + entry->d_name;
            if (name.substr(name.length() - 4) == ".txt")
            {
                if (FILE *file = fopen(name.c_str(), "r"))
                {
                    char line[512];
                    while (fgets(line, 512, file))
                    {
                        std::string str = line;
                        if (str.length() > 20 && str.substr(7, 13) == "song_name_en=")
                        {
                            // Set a song name from the database
                            std::string name = str.substr(20);
                            formatString(name);
                            songData[std::stoi(str.substr(3, 3))].name = name;
                        }
                        else if (str.length() > 20 && str.substr(7, 9) == "lyric_en.")
                        {
                            // Set a song lyric from the database
                            std::string lyric = str.substr(20);
                            formatString(lyric);
                            std::vector<std::string> &lyrics = songData[std::stoi(str.substr(3, 3))].lyrics;
                            size_t index = std::stoi(str.substr(16, 3));
                            lyrics.resize(index + 1);
                            lyrics[index] = lyric;
                        }
                        else if (str.length() > 41 && str.substr(7, 24) == "difficulty.easy.0.level=")
                        {
                            // Set an easy difficulty level as fixed-point with a 1-bit fractional
                            uint8_t diff = std::stoi(str.substr(37, 2)) * 2 + std::stoi(str.substr(40, 1)) / 5;
                            songData[std::stoi(str.substr(3, 3))].diffEasy = diff;
                        }
                        else if (str.length() > 43 && str.substr(7, 26) == "difficulty.normal.0.level=")
                        {
                            // Set a normal difficulty level as fixed-point with a 1-bit fractional
                            uint8_t diff = std::stoi(str.substr(39, 2)) * 2 + std::stoi(str.substr(42, 1)) / 5;
                            songData[std::stoi(str.substr(3, 3))].diffNorm = diff;
                        }
                        else if (str.length() > 41 && str.substr(7, 24) == "difficulty.hard.0.level=")
                        {
                            // Set a hard difficulty level as fixed-point with a 1-bit fractional
                            uint8_t diff = std::stoi(str.substr(37, 2)) * 2 + std::stoi(str.substr(40, 1)) / 5;
                            songData[std::stoi(str.substr(3, 3))].diffHard = diff;
                        }
                        else if (str.length() > 44 && str.substr(7, 27) == "difficulty.extreme.0.level=")
                        {
                            // Set an extreme difficulty level as fixed-point with a 1-bit fractional
                            uint8_t diff = std::stoi(str.substr(40, 2)) * 2 + std::stoi(str.substr(43, 1)) / 5;
                            songData[std::stoi(str.substr(3, 3))].diffExtr = diff;
                        }
                        else if (str.length() > 44 && str.substr(7, 27) == "difficulty.extreme.1.level=")
                        {
                            // Set an extra extreme difficulty level as fixed-point with a 1-bit fractional
                            uint8_t diff = std::stoi(str.substr(40, 2)) * 2 + std::stoi(str.substr(43, 1)) / 5;
                            songData[std::stoi(str.substr(3, 3))].diffExEx = diff;
                        }
                    }

                    fclose(file);
                }
            }
        }

        closedir(dir);
    }

    // Load saved score information from file if it exists
    if (FILE *file = fopen("/project-ds/scores.txt", "r"))
    {
        char line[512];
        while (fgets(line, 512, file))
        {
            std::string str = line;

            // Parse the difficulty index from the line
            uint8_t diff, len;
            for (diff = 0; diff < 4; diff++)
                if (str.substr(7, (len = diffs[diff].length())) == diffs[diff])
                    break;

            // Set the appropriate value based on the entry
            if (str.length() > 13 + len && str.substr(7 + len, 6) == "score=")
                songData[std::stoi(str.substr(3, 3))].scores[diff] = std::stoi(str.substr(13 + len));
            else if (str.length() > 13 + len && str.substr(7 + len, 6) == "clear=")
                songData[std::stoi(str.substr(3, 3))].clears[diff] = std::stof(str.substr(13 + len));
            else if (str.length() > 12 + len && str.substr(7 + len, 5) == "rank=")
                songData[std::stoi(str.substr(3, 3))].ranks[diff] = std::stoi(str.substr(12 + len));
        }

        fclose(file);
    }
}

void writeScores()
{
    // Write saved score information to file
    if (FILE *file = fopen("/project-ds/scores.txt", "w"))
    {
        for (size_t i = 0; i < 1000; i++)
        {
            for (uint8_t diff = 0; diff < 5; diff++)
            {
                // Add an entry for each non-zero score value
                if (songData[i].scores[diff])
                    fprintf(file, "pv_%03u.%sscore=%lu\n", i, diffs[diff].c_str(), songData[i].scores[diff]);
                if (songData[i].clears[diff])
                    fprintf(file, "pv_%03u.%sclear=%.2f\n", i, diffs[diff].c_str(), songData[i].clears[diff]);
                if (songData[i].ranks[diff])
                    fprintf(file, "pv_%03u.%srank=%u\n", i, diffs[diff].c_str(), songData[i].ranks[diff]);
            }
        }

        fclose(file);
    }
}
