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

#ifndef GAME_H
#define GAME_H

#include <cstdint>
#include <string>

struct Results
{
    float clear = 0;
    uint32_t total = 0;
    uint32_t cools = 0;
    uint32_t fines = 0;
    uint32_t safes = 0;
    uint32_t sads = 0;
    uint32_t misses = 0;
    uint32_t comboMax = 0;
    uint32_t scoreBase = 0;
    uint32_t scoreHold = 0;
    uint32_t scoreSlide = 0;
};

extern uint16_t *initObjBitmap(OamState *oam, const unsigned int *bitmap, size_t bitmapLen, SpriteSize size);

extern void gameInit();
extern void gameLoop();
extern void gameReset();

extern void loadChart(std::string &chartName, std::string &songName, size_t difficulty, bool retry);

#endif // GAME_H
