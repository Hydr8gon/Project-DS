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
#include <cmath>
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
#include "slider_lh.h"
#include "slider_l_hole.h"
#include "slider_lh_hole.h"
#include "slider_r.h"
#include "slider_rh.h"
#include "slider_r_hole.h"
#include "slider_rh_hole.h"
#include "cool.h"
#include "fine.h"
#include "safe.h"
#include "sad.h"
#include "miss.h"

#define PI 3.14159
#define FRAME_TIME 1672

void retry();

struct Note
{
    uint8_t type;
    int32_t x, y;
    int32_t incX, incY;
    int32_t ofsX, ofsY;
    uint32_t time;
};

std::deque<Note> notes;

size_t size = 0;
uint32_t *chart = nullptr;
uint32_t flyTimeDef = 1750;

FILE *song = nullptr;
mm_stream stream;

uint16_t *gfx[21];
int palIdx[21];

uint32_t counter = 1;
uint32_t timer = 0;
uint32_t flyTime = flyTimeDef * 100;
uint8_t life = 127;
uint8_t current = 0;
uint8_t mask = 0;
uint8_t mask2 = 0;
uint8_t statTimer = 0;
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
    if (index >= 16) return -1;
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
    printf("Press select to return to songs.");
    printf("Press dpad to adjust fly time.\n");
    mmStreamClose();

    // Show the retry screen
    while (true)
    {
        printf("\x1b[22;0HDefault fly time: %4lu\n", flyTimeDef);

        // Check key input once per frame
        scanKeys();
        uint16_t down = keysDown();
        swiWaitForVBlank();

        // Adjust default fly time with the up and down buttons
        // TODO: figure out how to properly set this
        if ((down & KEY_UP) && flyTimeDef < 3000)
            flyTimeDef += 50;
        else if ((down & KEY_DOWN) && flyTimeDef > 500)
            flyTimeDef -= 50;

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
        flyTime = flyTimeDef * 100;
        life = 127;
        current = 0;
        mask = 0;
        mask2 = 0;
        statTimer = 0;
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
                static uint32_t prev = 0;

                // Verify the note type
                if (chart[counter + 1] < 8) // Buttons and holds
                    note.type = chart[counter + 1] & 3;
                else if (chart[counter + 1] == 12 || chart[counter + 1] == 13) // Single slides
                    note.type = chart[counter + 1] - 8;
                else if (chart[counter + 1] == 15 || chart[counter + 1] == 16) // Held slides
                    note.type = (chart[counter + 1] - 11) | ((prev == chart[counter + 1]) << 7);
                else
                    break;

                prev = chart[counter + 1];

                // Calculate the note increment and offset based on angle and speed
                float angle = (int32_t)chart[counter + 4];
                angle = angle * PI / 180000;
                note.incX = (int)(sin(angle) *  0x100);
                note.incY = (int)(cos(angle) * -0x100);
                note.ofsX = note.incX * 60 * 3;
                note.ofsY = note.incY * 60 * 3;
                note.incX *= 100000.0f * 3 / flyTime;
                note.incY *= 100000.0f * 3 / flyTime;

                // Add a note to the queue
                note.x    = chart[counter + 2] * 256 / 480000 - 16;
                note.y    = chart[counter + 3] * 192 / 270000 - 16;
                note.time = timer + flyTime;
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

            case 0x3A: // Target flying time
            {
                // Set the time between a note's creation and when it should be hit
                flyTime = chart[counter + 1] * 100;
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
    gfx[0]  = initObjTiles16(triangle_holeTiles,  triangle_holeTilesLen,  SpriteSize_32x32);
    gfx[1]  = initObjTiles16(circle_holeTiles,    circle_holeTilesLen,    SpriteSize_32x32);
    gfx[2]  = initObjTiles16(cross_holeTiles,     cross_holeTilesLen,     SpriteSize_32x32);
    gfx[3]  = initObjTiles16(square_holeTiles,    square_holeTilesLen,    SpriteSize_32x32);
    gfx[4]  = initObjTiles16(slider_l_holeTiles,  slider_l_holeTilesLen,  SpriteSize_32x32);
    gfx[5]  = initObjTiles16(slider_r_holeTiles,  slider_r_holeTilesLen,  SpriteSize_32x32);
    gfx[6]  = initObjTiles16(slider_lh_holeTiles, slider_lh_holeTilesLen, SpriteSize_32x32);
    gfx[7]  = initObjTiles16(slider_rh_holeTiles, slider_rh_holeTilesLen, SpriteSize_32x32);
    gfx[8]  = initObjTiles16(triangleTiles,       triangleTilesLen,       SpriteSize_32x32);
    gfx[9]  = initObjTiles16(circleTiles,         circleTilesLen,         SpriteSize_32x32);
    gfx[10] = initObjTiles16(crossTiles,          crossTilesLen,          SpriteSize_32x32);
    gfx[11] = initObjTiles16(squareTiles,         squareTilesLen,         SpriteSize_32x32);
    gfx[12] = initObjTiles16(slider_lTiles,       slider_lTilesLen,       SpriteSize_32x32);
    gfx[13] = initObjTiles16(slider_rTiles,       slider_rTilesLen,       SpriteSize_32x32);
    gfx[14] = initObjTiles16(slider_lhTiles,      slider_lhTilesLen,      SpriteSize_32x32);
    gfx[15] = initObjTiles16(slider_rhTiles,      slider_rhTilesLen,      SpriteSize_32x32);
    gfx[16] = initObjTiles16(coolTiles,           coolTilesLen,           SpriteSize_32x16);
    gfx[17] = initObjTiles16(fineTiles,           fineTilesLen,           SpriteSize_32x16);
    gfx[18] = initObjTiles16(safeTiles,           safeTilesLen,           SpriteSize_32x16);
    gfx[19] = initObjTiles16(sadTiles,            sadTilesLen,            SpriteSize_32x16);
    gfx[20] = initObjTiles16(missTiles,           missTilesLen,           SpriteSize_32x16);

    // Prepare the graphic palette data
    palIdx[0]  = initObjPal16(triangle_holePal);
    palIdx[1]  = palIdx[0];
    palIdx[2]  = initObjPal16(cross_holePal);
    palIdx[3]  = palIdx[0];
    palIdx[4]  = initObjPal16(slider_l_holePal);
    palIdx[5]  = palIdx[4];
    palIdx[6]  = palIdx[4];
    palIdx[7]  = palIdx[4];
    palIdx[8]  = initObjPal16(trianglePal);
    palIdx[9]  = initObjPal16(circlePal);
    palIdx[10] = initObjPal16(crossPal);
    palIdx[11] = initObjPal16(squarePal);
    palIdx[12] = initObjPal16(slider_lPal);
    palIdx[13] = initObjPal16(slider_rPal);
    palIdx[14] = initObjPal16(slider_lhPal);
    palIdx[15] = initObjPal16(slider_rhPal);
    palIdx[16] = initObjPal16(coolPal);
    palIdx[17] = initObjPal16(finePal);
    palIdx[18] = initObjPal16(safePal);
    palIdx[19] = initObjPal16(sadPal);
    palIdx[20] = initObjPal16(missPal);

    int32_t statX = 0, statY = 0;
    int32_t statCurX = 0, statCurY = 0;
    uint8_t statType = 0;

    while (true)
    {
        // Update the song and chart
        mmStreamUpdate();
        updateChart();

        oamClear(&oamMain, 0, 0);
        int sprite = 0;

        scanKeys();
        uint16_t held = keysHeld();
        uint16_t down = keysDown();
        uint16_t up   = keysUp();

        if (!notes.empty() && notes[0].time - FRAME_TIME * 12 < timer)
        {
            // Get the keys that need to be pressed for the current notes
            if (!mask)
            {
                statX = statY = 0;
                while (current < notes.size() && notes[current].time == notes[0].time)
                {
                    statX += notes[current].x + 16;
                    statY += notes[current].y + 16;
                    mask |= BIT(notes[current++].type & ~BIT(7));
                }
                statX = statX / current - 16;
                statY = statY / current - 28;
            }

            if (mask)
            {
                // Scan key input and track which keys are pressed
                for (int i = 0; i < 6; i++)
                {
                    if ((notes[0].type & BIT(7)) && (held & keys[i]))
                    {
                        if (mask & BIT(i))
                            mask2 |= BIT(i);
                    }
                    else if (!(notes[0].type & BIT(7)) && (down & keys[i]))
                    {
                        if (mask & BIT(i))
                        {
                            mask2 |= BIT(i);
                            continue;
                        }

                        // Miss if a wrong key is pressed
                        statType = 4;
                        statTimer = 60;
                        statCurX = statX;
                        statCurY = statY;
                        life = std::max(0, life - 20);

                        // Clear notes that were missed
                        for (; current > 0; current--)
                            notes.pop_front();
                        mask = mask2 = 0;
                    }
                    else if (up & keys[i])
                    {
                        mask2 &= ~BIT(i);
                    }
                }

                // Abort if failed
                if (!mask)
                    continue;

                if (mask == mask2)
                {
                    if (!(notes[0].type & BIT(7)))
                    {
                        // Check how precisely the note was hit and adjust life
                        // TODO: verify timings
                        int32_t offset = abs((int32_t)(notes[0].time - timer));
                        if (offset <= FRAME_TIME * 3)
                        {
                            statType = 0; // Cool
                            life = std::min(255, life + 2);
                        }
                        else if (offset <= FRAME_TIME * 6)
                        {
                            statType = 1; // Fine
                            life = std::min(255, life + 1);
                        }
                        else if (offset <= FRAME_TIME * 9)
                        {
                            statType = 2; // Safe
                        }
                        else
                        {
                            statType = 3; // Sad
                            life = std::max(0, life - 10);
                        }

                        // Show the hit status above the note
                        statTimer = 60;
                        statCurX = statX;
                        statCurY = statY;
                    }

                    // Clear the notes that were hit
                    for (; current > 0; current--)
                        notes.pop_front();
                    mask = mask2 = 0;
                }
                else if (notes[0].time + FRAME_TIME * 12 < timer)
                {
                    // Miss if a note wasn't cleared in time
                    if (!(notes[0].type & BIT(7)))
                    {
                        statType = 4;
                        statTimer = 60;
                        statCurX = statX;
                        statCurY = statY;
                        life = std::max(0, life - 20);
                    }

                    // Clear the notes that were missed
                    for (; current > 0; current--)
                        notes.pop_front();
                    mask = mask2 = 0;
                }
            }
        }

        // Draw the hit status while its timer is active
        if (statTimer > 0)
        {
            oamSet(&oamMain, sprite++, statCurX, statCurY, 0, palIdx[16 + statType], SpriteSize_32x16,
                SpriteColorFormat_16Color, gfx[16 + statType], 0, false, false, false, false, false);
            statTimer--;
        }

        // Update all queued notes
        for (size_t i = 0; i < notes.size(); i++)
        {
            // Move the note closer to its hole
            int x = notes[i].x + ((notes[i].ofsX -= notes[i].incX) >> 8);
            int y = notes[i].y + ((notes[i].ofsY -= notes[i].incY) >> 8);

            // Draw the note if it's within screen bounds
            if (x > -32 && x < 256 && y > -32 && y < 192)
            {
                uint8_t type = (notes[i].type & ~BIT(7)) + ((notes[i].type & BIT(7)) ? 10 : 8);
                oamSet(&oamMain, sprite++, x, y, 0, palIdx[type], SpriteSize_32x32,
                    SpriteColorFormat_16Color, gfx[type], 0, false, false, false, false, false);
            }
        }

        // Draw holes for all queued notes
        for (size_t i = 0; i < notes.size(); i++)
        {
            uint8_t type = (notes[i].type & ~BIT(7)) + ((notes[i].type & BIT(7)) ? 2 : 0);
            oamSet(&oamMain, sprite++, notes[i].x, notes[i].y, 0, palIdx[type], SpriteSize_32x32,
                SpriteColorFormat_16Color, gfx[type], 0, false, false, false, false, false);
        }

        printf("\x1b[0;0HLife: %3u\n", life);

        // Move to the next frame
        oamUpdate(&oamMain);
        swiWaitForVBlank();
        timer += FRAME_TIME;

        // Check the end conditions
        if (down & KEY_START)
        {
            retry();
        }
        else if (life == 0)
        {
            printf("FAILED...\n");
            retry();
        }
        else if (finished && notes.empty())
        {
            printf("CLEAR!\n");
            retry();
        }
    }

    return 0;
}
