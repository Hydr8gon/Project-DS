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
#include <dirent.h>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "vorbis/codec.h"

int main()
{
    DIR *dir = opendir("ogg");
    std::vector<std::string> files;
    dirent *entry;

    // Build a list of all OGG files in the folder
    while (dir && (entry = readdir(dir)))
    {
        std::string name = entry->d_name;
        if (name.find(".ogg", name.length() - 4) != std::string::npos)
            files.push_back(name);
    }

    // Ensure there are files present
    if (files.empty())
    {
        printf("No OGG files found.\n");
        printf("Run this from the project-ds directory, with files in project-ds/ogg.\n");
        printf("Press enter to close the program.\n");
        getc(stdin);
        return 0;
    }

    closedir(dir);
    sort(files.begin(), files.end());
    mkdir("pcm", 0777);

    for (size_t k = 0; k < files.size(); k++)
    {
        // Infer names for all the files that might need to be accessed
        std::string oggName = "ogg/" + files[k].substr(0, 6) + ".ogg";
        std::string pcmName = "pcm/" + files[k].substr(0, 6) + ".pcm";

        // Attempt to load an OGG file for conversion
        if (FILE *oggFile = fopen(oggName.c_str(), "rb"))
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
            size_t sizeOgg = ftell(oggFile);
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

            printf("Converting file %d of %d...\n", k + 1, files.size());

            int i = 0;
            FILE *pcmFile = fopen(pcmName.c_str(), "wb");

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
                        fwrite(convbuffer, sizeof(int16_t), samples * 2, pcmFile);
                        vorbis_synthesis_read(&vd, samples);
                    }
                }
            }

            fclose(pcmFile);
            fclose(oggFile);
        }
    }

    printf("Done!\n");
    printf("Press enter to close the program.\n");
    getc(stdin);
    return 0;
}
