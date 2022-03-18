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

#include <fat.h>
#include <maxmod9.h>
#include <nds.h>
#include <sys/stat.h>

#include "game.h"
#include "menu.h"

int main()
{
    fatInitDefault();
    consoleDemoInit();

    // Create directories in case they don't exist
    mkdir("/project-ds",     0777);
    mkdir("/project-ds/dsc", 0777);
    mkdir("/project-ds/ogg", 0777);
    mkdir("/project-ds/pcm", 0777);

    // Setup graphics on the main screen
    videoSetMode(MODE_3_2D);
    vramSetBankA(VRAM_A_MAIN_SPRITE);
    oamInit(&oamMain, SpriteMapping_Bmp_1D_128, false);
    BG_PALETTE[0] = 0x4210;

    // Initialize maxmod without a soundbank
    mm_ds_system sys;
    sys.mod_count    = 0;
    sys.samp_count   = 0;
    sys.mem_bank     = 0;
    sys.fifo_channel = FIFO_MAXMOD;
    mmInit(&sys);

    // Run the game
    gameInit();
    gameLoop();

    return 0;
}
