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

#ifndef AUDIO_H
#define AUDIO_H

#include <string>

extern void audioInit();
extern void setLagConfig(int ms);

extern void playSong(std::string &name);
extern void resumeSong();
extern void updateSong();
extern void stopSong();

extern bool convertSong(std::string &src, std::string &dst);

#endif // AUDIO_H
