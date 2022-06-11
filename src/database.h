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

#ifndef DATABASE_H
#define DATABASE_H

#include <string>

struct SongData
{
    std::string name;
    std::string lyrics[128];
};

extern SongData songData[1000];

extern void databaseInit();

#endif // DATABASE_H
