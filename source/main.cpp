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

#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>

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

FILE *song;
mm_stream stream;

uint16_t *gfx[8];
int palIdx[8];

uint32_t counter = 1;
uint32_t timer = 0;
bool finished = false;

const uint8_t paramCounts[0x100] =
{
    0,  1,  4,  2,  2,  2,  7,  4,  2,  6,  2,  1,  6,  2,  1,  1,
    3,  2,  3,  5,  5,  4,  4,  5,  2,  0,  2,  4,  2,  2,  1, 21,
    0,  3,  2,  5,  1,  1,  7,  1,  1,  2,  1,  2,  1,  2,  3,  3,
    1,  2,  2,  3,  6,  6,  1,  1,  2,  3,  1,  2,  2,  4,  4,  1,
    2,  1,  2,  1,  1,  3,  3,  3,  2,  1,  9,  3,  2,  4,  2,  3,
    2, 24,  1,  2,  1,  3,  1,  3,  4,  1,  2,  6,  3,  2,  3,  3,
    4,  1,  1,  3,  3,  4,  2,  3,  3,  8,  2
};

const uint16_t keys[4] =
{
    (KEY_X | KEY_UP),   (KEY_A | KEY_RIGHT), // Triangle, Circle
    (KEY_B | KEY_DOWN), (KEY_Y | KEY_LEFT)   // Cross,    Square
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
                // Add a note to the queue
                if (chart[counter + 1] < 8) // Buttons and holds
                {
                    static Note note;
                    note.type = chart[counter + 1] & 3;
                    note.x    = chart[counter + 2] * 256 / 500000 - 16;
                    note.y    = chart[counter + 3] * 192 / 250000 - 16;
                    note.time = timer + 150000;
                    notes.push_back(note);
                }
                break;
            }

            case 0x19: // Music play
            {
                // Start the audio stream
                mmStreamOpen(&stream);
                break;
            }
        }

        // Move to the next opcode
        counter += paramCounts[chart[counter]] + 1;
    }
}

void retry()
{
    printf("Press start to retry.\n");
    mmStreamClose();

    // Wait on the retry screen
    while (true)
    {
        scanKeys();

        // Reset the chart when start is pressed
        if (keysDown() & KEY_START)
        {
            consoleClear();
            notes.clear();
            fseek(song, 0, SEEK_SET);
            counter = 1;
            timer = 0;
            finished = false;
            return;
        }

        swiWaitForVBlank();
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

    // Attempt to load the chart file
    if (FILE *file = fopen("chart.dsc", "rb"))
    {
        // Load the chart file into memory
        fseek(file, 0, SEEK_END);
        size = ftell(file) / 4;
        fseek(file, 0, SEEK_SET);
        chart = new uint32_t[size];
        fread(chart, sizeof(uint32_t), size, file);
        fclose(file);
    }
    else
    {
        printf("Please provide a file named chart.dsc on the SD card.\n");

        // Do nothing since there's no chart to play
        while (true)
            swiWaitForVBlank();
    }

    // Attempt to load the converted music file
    if (!(song = fopen("chart.pcm", "rb")))
    {
        // Attempt to load an OGG file for conversion
        if (FILE *songOgg = fopen("chart.ogg", "rb"))
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
            FILE *songPcm = fopen("chart.pcm", "wb");

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
                        int16_t convbuffer[2048];
                        for (int j = 0; j < samples; j++)
                        {
                            convbuffer[j * 2 + 0] = (pcm[0][j] + pcm[2][j]) * 32767.0f;
                            convbuffer[j * 2 + 1] = (pcm[1][j] + pcm[3][j]) * 32767.0f;
                        }

                        // Write the converted data to file
                        fwrite(convbuffer, sizeof(int16_t), samples * 2, songPcm);
                        vorbis_synthesis_read(&vd, samples);
                    }
                }
            }

            fclose(songPcm);
            fclose(songOgg);

            // Open the file after decoding
            song = fopen("chart.pcm", "rb");
            printf("Done!\n");
            retry();
        }
        else
        {
            printf("Please provide a file named chart.ogg on the SD card.\n");

            // Do nothing since there's no music to play
            while (true)
                swiWaitForVBlank();
        }
    }

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
    gfx[0] = initObjTiles16(triangle_holeTiles, triangle_holeTilesLen, SpriteSize_32x32);
    gfx[1] = initObjTiles16(circle_holeTiles,   circle_holeTilesLen,   SpriteSize_32x32);
    gfx[2] = initObjTiles16(cross_holeTiles,    cross_holeTilesLen,    SpriteSize_32x32);
    gfx[3] = initObjTiles16(square_holeTiles,   square_holeTilesLen,   SpriteSize_32x32);
    gfx[4] = initObjTiles16(triangleTiles,      triangleTilesLen,      SpriteSize_32x32);
    gfx[5] = initObjTiles16(circleTiles,        circleTilesLen,        SpriteSize_32x32);
    gfx[6] = initObjTiles16(crossTiles,         crossTilesLen,         SpriteSize_32x32);
    gfx[7] = initObjTiles16(squareTiles,        squareTilesLen,        SpriteSize_32x32);

    // Prepare the graphic palette data
    palIdx[0] = initObjPal16(triangle_holePal);
    palIdx[1] = initObjPal16(circle_holePal);
    palIdx[2] = initObjPal16(cross_holePal);
    palIdx[3] = initObjPal16(square_holePal);
    palIdx[4] = initObjPal16(trianglePal);
    palIdx[5] = initObjPal16(circlePal);
    palIdx[6] = initObjPal16(crossPal);
    palIdx[7] = initObjPal16(squarePal);

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

            // Get the current notes that need to be hit
            size_t count = 0;
            uint16_t key = 0;
            for (; count < notes.size() && notes[count].time == notes[0].time; count++)
                key |= keys[notes[count].type];

            if (down && key)
            {
                if (down & key)
                {
                    // Dequeue notes that are hit
                    for (size_t i = 0; i < count; i++)
                        notes.pop_front();
                }
                else
                {
                    // Fail if the pressed key is wrong
                    printf("FAILED...\n");
                    retry();
                }
            }
            else if (notes[0].time + 50000 < timer)
            {
                // Fail if a note wasn't hit in time
                printf("FAILED...\n");
                retry();
            }
            else
            {
                // Draw buttons for the current notes
                for (size_t i = 0; i < count; i++)
                {
                    oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, palIdx[notes[i].type + 4], SpriteSize_32x32,
                        SpriteColorFormat_16Color, gfx[notes[i].type + 4], 0, false, false, false, false, false);
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
        timer += 1667;

        // Check the clear condition
        if (finished && notes.empty())
        {
            printf("CLEAR!\n");
            retry();
        }
    }

    return 0;
}
