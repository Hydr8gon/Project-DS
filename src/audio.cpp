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

#include <maxmod9.h>
#include <nds.h>

#include "vorbis/codec.h"

#include "audio.h"

static mm_stream stream;
static FILE *song = nullptr;

static mm_word audioCallback(mm_word length, mm_addr dest, mm_stream_formats format)
{
    // Load more PCM samples from file
    fread(dest, sizeof(int16_t), length * 2, song);
    return length;
}

void audioInit()
{
    // Prepare the audio stream
    stream.sampling_rate = 44100 / 2;
    stream.buffer_length = 1024;
    stream.callback      = audioCallback;
    stream.format        = MM_STREAM_16BIT_STEREO;
    stream.timer         = MM_TIMER0;
    stream.manual        = true;
}

void playSong(std::string &name)
{
    // Open and play a PCM file if it exists
    if (!song && (song = fopen(name.c_str(), "rb")))
        mmStreamOpen(&stream);
}

void updateSong()
{
    // Update the PCM stream if it's running
    if (song)
        mmStreamUpdate();
}

void stopSong()
{
    // Stop the PCM stream if it's running
    if (song)
    {
        mmStreamClose();
        fclose(song);
        song = nullptr;
    }
}

bool convertSong(std::string &src, std::string &dst)
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
                    int16_t conv[2048] = {};
                    for (int j = 0; j < samples; j++)
                    {
                        for (int c = 0; c < vi.channels; c += 2)
                        {
                            conv[j * 2 + 0] += pcm[c + 0][j] * 32767.0f;
                            conv[j * 2 + 1] += pcm[c + 1][j] * 32767.0f;
                        }
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
