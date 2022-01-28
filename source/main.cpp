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
#include <cstdint>
#include <cstdio>
#include <deque>
#include <dirent.h>
#include <string>
#include <vector>

#include <fat.h>
#include <maxmod9.h>
#include <nds.h>

#include "vorbis/codec.h"

#include "circle.h"
#include "circle_hole.h"
#include "cross.h"
#include "cross_hole.h"
#include "square.h"
#include "square_hole.h"
#include "triangle.h"
#include "triangle_hole.h"
#include "slider_l.h"
#include "slider_l_hole.h"
#include "slider_r.h"
#include "slider_r_hole.h"

void retry();

struct Note
{
    uint8_t type;
    uint8_t x;
    uint8_t y;
    uint32_t time;
};

std::deque<Note> notes;

size_t size = 0;
uint32_t *chart = nullptr;

FILE *song = nullptr;
mm_stream stream;

uint16_t *gfx[12];
int palIdx[12];

uint32_t counter = 1;
uint32_t timer = 0;
uint8_t current = 0;
uint8_t mask = 0;
uint8_t mask2 = 0;
bool finished = false;

const uint8_t paramCounts[0x100] =
{
    0,  1,  4,  2,  2,  2,  7,  4,  2,  6,  2,  1,  6,  2,  1,  1, // 0x00-0x0F
    3,  2,  3,  5,  5,  4,  4,  5,  2,  0,  2,  4,  2,  2,  1, 21, // 0x10-0x1F
    0,  3,  2,  5,  1,  1,  7,  1,  1,  2,  1,  2,  1,  2,  3,  3, // 0x20-0x2F
    1,  2,  2,  3,  6,  6,  1,  1,  2,  3,  1,  2,  2,  4,  4,  1, // 0x30-0x3F
    2,  1,  2,  1,  1,  3,  3,  3,  2,  1,  9,  3,  2,  4,  2,  3, // 0x40-0x4F
    2, 24,  1,  2,  1,  3,  1,  3,  4,  1,  2,  6,  3,  2,  3,  3, // 0x50-0x5F
    4,  1,  1,  3,  3,  4,  2,  3,  3,  8,  2                      // 0x60-0x6A
};

const uint16_t keys[6] =
{
    (KEY_X | KEY_UP),   (KEY_A | KEY_RIGHT), // Triangle, Circle
    (KEY_B | KEY_DOWN), (KEY_Y | KEY_LEFT),  // Cross,    Square
    KEY_L,              KEY_R                // Slider L, Slider R
};

uint16_t *initObjTiles16(const unsigned int *tiles, size_t tilesLen, SpriteSize size)
{
    // Copy 16-color object tiles into appropriate memory, and return a pointer to the data
    uint16_t *gfx = oamAllocateGfx(&oamMain, size, SpriteColorFormat_16Color);
    if (gfx) dmaCopy(tiles, gfx, tilesLen);
    return gfx;
}

int initObjPal16(const unsigned short *pal)
{
    // Copy a 16-color object palette into appropriate memory, and return the palette index
    static uint8_t index = 0;
    if (index >= 32) return -1;
    dmaCopy(pal, &SPRITE_PALETTE[index * 16], 16 * sizeof(uint16_t));
    return index++;
}

mm_word audioCallback(mm_word length, mm_addr dest, mm_stream_formats format)
{
    // Load more PCM samples from file
    fread(dest, sizeof(int16_t), length * 2, song);
    return length;
}

void fileBrowser()
{
    std::vector<std::string> files;
    DIR *dir = opendir("/project-ds/dsc");
    dirent *entry;

    // Build a list of all DSC files in the folder
    while ((entry = readdir(dir)))
    {
        std::string name = entry->d_name;
        if (name.find(".dsc", name.length() - 4) != std::string::npos)
            files.push_back(name);
    }

    closedir(dir);
    sort(files.begin(), files.end());

    // Ensure there are files present
    if (files.empty())
    {
        printf("No DSC files found.\n");
        printf("Place them in '/project-ds/dsc'.\n");

        // Do nothing since there are no files to show
        while (true)
            swiWaitForVBlank();
    }

    size_t selection = 0;
    uint8_t framesHeld = 0;

    // Show the file browser
    while (true)
    {
        // Calculate the offset to display the files from
        size_t offset = 0;
        if (files.size() > 23)
        {
            if (selection >= files.size() - 11)
                offset = files.size() - 23;
            else if (selection > 11)
                offset = selection - 11;
        }

        // Display a section of files around the current selection
        consoleClear();
        for (size_t i = offset; i < offset + std::min(files.size(), 23U); i++)
            printf((i == selection) ? "\x1b[%d;0H>%s\n" : "\x1b[%d;0H %s\n", i - offset, files[i].c_str());

        uint16_t held = 0;

        // Wait for button input
        while (!held)
        {
            scanKeys();
            held = keysHeld();
            if (!held) framesHeld = 0;
            swiWaitForVBlank();
        }

        if (held & KEY_A)
        {
            // Select the current file and proceed to load it
            consoleClear();
            break;
        }
        else if (held & KEY_UP)
        {
            // Decrement the current selection with wraparound, continuously after 30 frames
            if ((framesHeld > 30 || framesHeld++ == 0) && selection-- == 0)
                selection = files.size() - 1;
        }
        else if (held & KEY_DOWN)
        {
            // Increment the current selection with wraparound, continuously after 30 frames
            if ((framesHeld > 30 || framesHeld++ == 0) && ++selection == files.size())
                selection = 0;
        }
    }

    // Infer names for all the files that might need to be accessed
    std::string dscName = "/project-ds/dsc/" + files[selection];
    std::string oggName = "/project-ds/ogg/" + files[selection].substr(0, 6) + ".ogg";
    std::string pcmName = "/project-ds/pcm/" + files[selection].substr(0, 6) + ".pcm";

    // Load the chart file into memory
    FILE *file = fopen(dscName.c_str(), "rb");
    fseek(file, 0, SEEK_END);
    size = ftell(file) / 4;
    fseek(file, 0, SEEK_SET);
    if (chart) delete[] chart;
    chart = new uint32_t[size];
    fread(chart, sizeof(uint32_t), size, file);
    fclose(file);

    if (song)
    {
        fclose(song);
        song = nullptr;
    }

    // Attempt to load the converted music file
    if (!(song = fopen(pcmName.c_str(), "rb")))
    {
        // Attempt to load an OGG file for conversion
        if (FILE *songOgg = fopen(oggName.c_str(), "rb"))
        {
            ogg_sync_state   oy;
            ogg_stream_state os;
            ogg_page         og;
            ogg_packet       op;

            vorbis_info      vi;
            vorbis_comment   vc;
            vorbis_dsp_state vd;
            vorbis_block     vb;

            // Get the file size for progress tracking
            fseek(songOgg, 0, SEEK_END);
            size_t sizeOgg = ftell(songOgg);
            fseek(songOgg, 0, SEEK_SET);

            // Read the first block from the OGG file
            ogg_sync_init(&oy);
            char *buffer = ogg_sync_buffer(&oy, 4096);
            size_t bytes = fread(buffer, sizeof(uint8_t), 4096, songOgg);
            ogg_sync_wrote(&oy, bytes);

            // Initialize the stream and get the first page
            ogg_sync_pageout(&oy, &og);
            ogg_stream_init(&os, ogg_page_serialno(&og));
            ogg_stream_pagein(&os, &og);
            ogg_stream_packetout(&os, &op);

            // Initialize the decoder with the initial header
            vorbis_info_init(&vi);
            vorbis_comment_init(&vc);
            vorbis_synthesis_headerin(&vi, &vc, &op);
            vorbis_synthesis_halfrate(&vi, 1);

            printf("Converting to PCM16...\n");
            FILE *songPcm = fopen(pcmName.c_str(), "wb");

            int i = 0;

            // Decode until the end of the file is reached
            while (true)
            {
                // Read more blocks from the OGG file and track progress
                if (!ogg_sync_pageout(&oy, &og))
                {
                    buffer = ogg_sync_buffer(&oy, 4096);
                    bytes = fread(buffer, sizeof(uint8_t), 4096, songOgg);
                    if (bytes == 0) break;
                    ogg_sync_wrote(&oy, bytes);
                    printf("\x1b[1;0H%ld%%\n", ftell(songOgg) * 100 / sizeOgg);
                    continue;
                }

                // Get another page
                ogg_stream_pagein(&os, &og);

                while (ogg_stream_packetout(&os, &op))
                {
                    // Get the comment and codebook headers and finish initializing the decoder
                    if (i < 2)
                    {
                        vorbis_synthesis_headerin(&vi, &vc, &op);
                        if (++i < 2) continue;
                        vorbis_synthesis_init(&vd, &vi);
                        vorbis_block_init(&vd, &vb);
                        break;
                    }

                    // Decode a packet
                    vorbis_synthesis(&vb, &op);
                    vorbis_synthesis_blockin(&vd, &vb);

                    float **pcm;

                    while (int samples = vorbis_synthesis_pcmout(&vd, &pcm))
                    {
                        // Convert floats and combine channels to produce stereo PCM16
                        int16_t conv[2048];
                        for (int j = 0; j < samples; j++)
                        {
                            conv[j * 2 + 0] = (pcm[0][j] + pcm[2][j]) * 32767.0f;
                            conv[j * 2 + 1] = (pcm[1][j] + pcm[3][j]) * 32767.0f;
                        }

                        // Write the converted data to file
                        fwrite(conv, sizeof(int16_t), samples * 2, songPcm);
                        vorbis_synthesis_read(&vd, samples);
                    }
                }
            }

            fclose(songPcm);
            fclose(songOgg);

            // Open the file after decoding
            song = fopen(pcmName.c_str(), "rb");
            printf("Done!\n");
            retry();
        }
    }
}

void retry()
{
    printf("Press start to retry.\n");
    printf("Press select to return to songs.\n");
    mmStreamClose();

    // Show the retry screen
    while (true)
    {
        // Check key input once per frame
        scanKeys();
        uint16_t down = keysDown();
        swiWaitForVBlank();

        // Open the file browser on select, or reset the chart on start
        if (down & KEY_SELECT)
            fileBrowser();
        else if (!(down & KEY_START))
            continue;

        // Reset the current chart
        consoleClear();
        notes.clear();
        fseek(song, 0, SEEK_SET);
        counter = 1;
        timer = 0;
        current = 0;
        mask = 0;
        mask2 = 0;
        finished = false;
        return;
    }
}

void updateChart()
{
    // Execute chart opcodes
    while (counter < size && !finished)
    {
        switch (chart[counter])
        {
            case 0x00: // End
            {
                // Indicate the chart has finished executing
                finished = true;
                return;
            }

            case 0x01: // Time
            {
                // Stop execution until the target time is reached
                if (timer < chart[counter + 1])
                    return;
                break;
            }

            case 0x06: // Target
            {
                static Note note;

                // Verify the note type
                if (chart[counter + 1] < 8) // Buttons and holds
                    note.type = chart[counter + 1] & 3;
                else if (chart[counter + 1] == 12 || chart[counter + 1] == 13) // Single slides
                    note.type = chart[counter + 1] - 8;
                else
                    break;

                // Add a note to the queue
                note.x    = chart[counter + 2] * 256 / 480000 - 16;
                note.y    = chart[counter + 3] * 192 / 270000 - 16;
                note.time = timer + 150000;
                notes.push_back(note);
                break;
            }

            case 0x19: // Music play
            {
                // Start the audio stream if a file is loaded
                if (song)
                    mmStreamOpen(&stream);
                break;
            }
        }

        // Move to the next opcode
        counter += paramCounts[chart[counter]] + 1;
    }
}

int main()
{
    fatInitDefault();
    consoleDemoInit();

    // Setup graphics on the main screen
    videoSetMode(MODE_3_2D);
    vramSetBankA(VRAM_A_MAIN_SPRITE);
    oamInit(&oamMain, SpriteMapping_1D_64, false);
    BG_PALETTE[0] = 0x4210;

    // Create directories in case they don't exist
    mkdir("/project-ds",     0777);
    mkdir("/project-ds/dsc", 0777);
    mkdir("/project-ds/ogg", 0777);
    mkdir("/project-ds/pcm", 0777);

    // Open the file browser
    fileBrowser();

    // Initialize maxmod without a soundbank
    mm_ds_system sys;
    sys.mod_count    = 0;
    sys.samp_count   = 0;
    sys.mem_bank     = 0;
    sys.fifo_channel = FIFO_MAXMOD;
    mmInit(&sys);

    // Prepare the audio stream
    stream.sampling_rate = 44100 / 2;
    stream.buffer_length = 1024;
    stream.callback      = audioCallback;
    stream.format        = MM_STREAM_16BIT_STEREO;
    stream.timer         = MM_TIMER0;
    stream.manual        = true;

    // Prepare the graphic tile data
    gfx[0]  = initObjTiles16(triangle_holeTiles, triangle_holeTilesLen, SpriteSize_32x32);
    gfx[1]  = initObjTiles16(circle_holeTiles,   circle_holeTilesLen,   SpriteSize_32x32);
    gfx[2]  = initObjTiles16(cross_holeTiles,    cross_holeTilesLen,    SpriteSize_32x32);
    gfx[3]  = initObjTiles16(square_holeTiles,   square_holeTilesLen,   SpriteSize_32x32);
    gfx[4]  = initObjTiles16(slider_l_holeTiles, slider_l_holeTilesLen, SpriteSize_32x32);
    gfx[5]  = initObjTiles16(slider_r_holeTiles, slider_r_holeTilesLen, SpriteSize_32x32);
    gfx[6]  = initObjTiles16(triangleTiles,      triangleTilesLen,      SpriteSize_32x32);
    gfx[7]  = initObjTiles16(circleTiles,        circleTilesLen,        SpriteSize_32x32);
    gfx[8]  = initObjTiles16(crossTiles,         crossTilesLen,         SpriteSize_32x32);
    gfx[9]  = initObjTiles16(squareTiles,        squareTilesLen,        SpriteSize_32x32);
    gfx[10] = initObjTiles16(slider_lTiles,      slider_lTilesLen,      SpriteSize_32x32);
    gfx[11] = initObjTiles16(slider_rTiles,      slider_rTilesLen,      SpriteSize_32x32);

    // Prepare the graphic palette data
    palIdx[0]  = initObjPal16(triangle_holePal);
    palIdx[1]  = initObjPal16(circle_holePal);
    palIdx[2]  = initObjPal16(cross_holePal);
    palIdx[3]  = initObjPal16(square_holePal);
    palIdx[4]  = initObjPal16(slider_l_holePal);
    palIdx[5]  = initObjPal16(slider_r_holePal);
    palIdx[6]  = initObjPal16(trianglePal);
    palIdx[7]  = initObjPal16(circlePal);
    palIdx[8]  = initObjPal16(crossPal);
    palIdx[9]  = initObjPal16(squarePal);
    palIdx[10] = initObjPal16(slider_lPal);
    palIdx[11] = initObjPal16(slider_rPal);

    while (true)
    {
        // Update the song and chart
        mmStreamUpdate();
        updateChart();

        oamClear(&oamMain, 0, 0);
        int sprite = 0;

        if (!notes.empty() && notes[0].time - 50000 < timer)
        {
            scanKeys();
            uint16_t down = keysDown();
            uint16_t up   = keysUp();

            // Get the keys that need to be pressed for the current notes
            if (!mask)
            {
                while (current < notes.size() && notes[current].time == notes[0].time)
                    mask |= BIT(notes[current++].type);
            }

            if (mask)
            {
                // Scan key input and track which keys are pressed
                for (int i = 0; i < 6; i++)
                {
                    if (down & keys[i])
                    {
                        if (mask & BIT(i))
                        {
                            mask2 |= BIT(i);
                            continue;
                        }

                        // Fail if a wrong key is pressed
                        printf("FAILED...\n");
                        retry();
                    }
                    else if (up & keys[i])
                    {
                        mask2 &= ~BIT(i);
                    }
                }

                if (mask == mask2)
                {
                    // Clear notes if the pressed keys match
                    for (; current > 0; current--)
                        notes.pop_front();
                    mask = mask2 = 0;
                }
                else if (notes[0].time + 50000 < timer)
                {
                    // Fail if a note wasn't cleared in time
                    printf("FAILED...\n");
                    retry();
                }
                else
                {
                    // Draw buttons for the current notes
                    for (size_t i = 0; i < current; i++)
                    {
                        oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, palIdx[notes[i].type + 6], SpriteSize_32x32,
                            SpriteColorFormat_16Color, gfx[notes[i].type + 6], 0, false, false, false, false, false);
                    }
                }
            }
        }

        // Draw holes for all queued notes
        for (size_t i = 0; i < notes.size(); i++)
        {
            oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, palIdx[notes[i].type], SpriteSize_32x32,
                SpriteColorFormat_16Color, gfx[notes[i].type], 0, false, false, false, false, false);
        }

        // Move to the next frame
        oamUpdate(&oamMain);
        swiWaitForVBlank();
        timer += 1670;

        // Check the clear condition
        if (finished && notes.empty())
        {
            printf("CLEAR!\n");
            retry();
        }
    }

    return 0;
}
