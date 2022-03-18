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
#include <string>
#include <vector>

#include <nds.h>

#include "menu.h"
#include "game.h"

#include "vorbis/codec.h"

static bool convertSong(std::string &src, std::string &dst)
{
    // Attempt to load an OGG file for conversion
    if (FILE *oggFile = fopen(src.c_str(), "rb"))
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
        fseek(oggFile, 0, SEEK_END);
        size_t oggSize = ftell(oggFile);
        fseek(oggFile, 0, SEEK_SET);

        // Read the first block from the OGG file
        ogg_sync_init(&oy);
        char *buffer = ogg_sync_buffer(&oy, 4096);
        size_t bytes = fread(buffer, sizeof(uint8_t), 4096, oggFile);
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

        int i = 0;
        FILE *pcmFile = fopen(dst.c_str(), "wb");

        // Decode until the end of the file is reached
        while (true)
        {
            // Read more blocks from the OGG file and track progress
            if (!ogg_sync_pageout(&oy, &og))
            {
                buffer = ogg_sync_buffer(&oy, 4096);
                bytes = fread(buffer, sizeof(uint8_t), 4096, oggFile);
                if (bytes == 0) break;
                ogg_sync_wrote(&oy, bytes);
                printf("\x1b[1;0H%ld%%\n", ftell(oggFile) * 100 / oggSize);
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
                    fwrite(conv, sizeof(int16_t), samples * 2, pcmFile);
                    vorbis_synthesis_read(&vd, samples);
                }
            }
        }

        fclose(pcmFile);
        fclose(oggFile);

        printf("Done!\n");
        return true;
    }

    return false;
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

    // Open the chart and converted song files
    FILE *chart = fopen(dscName.c_str(), "rb");
    FILE *song  = fopen(pcmName.c_str(), "rb");
    bool retry  = false;

    // Try to convert the song if it wasn't found
    if (!song && convertSong(oggName, pcmName))
    {
        song = fopen(pcmName.c_str(), "rb");
        retry = true;
    }

    loadChart(chart, song, retry);
}
