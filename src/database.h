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

#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>

struct SongData
{
    std::string name;
    std::vector<std::string> lyrics;

    union
    {
        struct __attribute__((packed))
        {
            uint32_t diffEasy : 5;
            uint32_t diffNorm : 5;
            uint32_t diffHard : 5;
            uint32_t diffExtr : 5;
            uint32_t diffExEx : 5;
        };

        uint32_t difficulty = 0;
    };

    uint32_t scores[5] = {};
    float clears[5] = {};
    uint8_t ranks[5] = {};
};

extern SongData songData[1000];

extern void databaseInit();
extern void writeScores();

#endif // DATABASE_H
