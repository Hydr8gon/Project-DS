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

#include <nds.h>

#include "database.h"

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
    // Scan database files (.txt) for English song information
    if (DIR *dir = opendir("/project-ds"))
    {
        while (dirent *entry = readdir(dir))
        {
            std::string name = (std::string)"/project-ds/" + entry->d_name;
            if (name.substr(name.length() - 4) == ".txt")
            {
                if (FILE *file = fopen(name.c_str(), "r"))
                {
                    char line[512];
                    while (fgets(line, 512, file))
                    {
                        std::string str = line;
                        if (str.length() > 20 && str.substr(7, 12) == "song_name_en")
                        {
                            // Set a song name from the database
                            std::string name = str.substr(20);
                            formatString(name);
                            songData[std::stoi(str.substr(3, 3))].name = name.substr(0, 31);
                        }
                        else if (str.length() > 20 && str.substr(7, 8) == "lyric_en")
                        {
                            // Set a song lyric from the database
                            std::string lyric = str.substr(20);
                            formatString(lyric);
                            songData[std::stoi(str.substr(3, 3))].lyrics.push_back(lyric);
                        }
                    }

                    fclose(file);
                }
            }
        }

        closedir(dir);
    }
}
