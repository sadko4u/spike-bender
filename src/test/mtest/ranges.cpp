/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of spike-bender
 * Created on: 1 апр. 2023 г.
 *
 * spike-bender is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * spike-bender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with spike-bender. If not, see <https://www.gnu.org/licenses/>.
 */


#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/stdlib/stdio.h>
#include <lsp-plug.in/test-fw/mtest.h>

#include <private/audio.h>

#define SAMPLE_RATE         48000

using namespace spike_bender;

MTEST_BEGIN("spike_bender", ranges)

    void save_ranges(lltl::darray<range_t> *ranges, const io::Path *path)
    {
        FILE *fd;

        MTEST_ASSERT((fd = fopen(path->as_native(), "w")) != NULL);

        fprintf(fd, "index;first;last;peak;gain;gain_db;\n");

        for (size_t i=0, n=ranges->size(); i<n; ++i)
        {
            range_t *r  = ranges->uget(i);

            if (r->gain > 1e-6)
                fprintf(fd, "%d;%d;%d;%d;%.5f;%.2f;\n",
                    int(i), int(r->first), int(r->last), int(r->peak), r->gain, dspu::gain_to_db(r->gain));
            else
                fprintf(fd, "%d;%d;%d;%d;%.5f;-inf;\n",
                    int(i), int(r->first), int(r->last), int(r->peak), r->gain);
        }

        MTEST_ASSERT(fclose(fd) == STATUS_OK);
    }

    void make_range_mapping(lsp::dspu::Sample *out, lltl::darray<range_t> *ranges, size_t length)
    {
        MTEST_ASSERT(out->init(1, length+1, length+1));
        out->set_sample_rate(SAMPLE_RATE);

        float *dst = out->channel(0);

        for (size_t i=0, n=ranges->size(); i<n; ++i)
        {
            range_t *r  = ranges->uget(i);

            dst[r->first]   = -1.0f;
            dst[r->peak]    = r->gain;
            dst[r->last]    = -1.0f;
        }
    }

    MTEST_MAIN
    {

        lsp::dspu::Sample in, rms, win, image;
        io::Path path;

        //----------------------------------------------------------------------------
        // ROUND 0
        // STEP 0: Load the source audio file
        MTEST_ASSERT(path.fmt("%s/samples/in/test.wav", resources()) > 0);
        MTEST_ASSERT(load_audio_file(&in, path.as_string(), SAMPLE_RATE) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/range/00-source.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&in, path.as_string()) == STATUS_OK);

        // STEP 1: Apply weighting function
        MTEST_ASSERT(spike_bender::apply_weight(&win, &in, spike_bender::K_WEIGHT) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/range/01-weighted.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&win, path.as_string()) == STATUS_OK);

        // STEP 2: Estmate RMS
        size_t period   = dspu::millis_to_samples(SAMPLE_RATE, 100.0f);
        MTEST_ASSERT(spike_bender::estimate_rms(&rms, &in, spike_bender::K_WEIGHT, period) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/range/02-rms.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&rms, path.as_string()) == STATUS_OK);

        //----------------------------------------------------------------------------
        // ROUND 1
        // STEP 1: Lookup all ranges
        lltl::darray<range_t> ranges;

        MTEST_ASSERT(find_peaks(&ranges, win.channel(0), rms.channel(0), dspu::db_to_gain(-48.0f), win.length()) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/range/03-ranges.csv", resources()) > 0);
        save_ranges(&ranges, &path);
        make_range_mapping(&image, &ranges, win.length());

        // STEP 2: Save produced sample
        MTEST_ASSERT(path.fmt("%s/samples/range/04-ranges.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&image, path.as_string()) == STATUS_OK);

        // STEP 3: Apply gain
        apply_gain(win.channel(0), &ranges, dspu::db_to_gain(-48.0f));
        MTEST_ASSERT(path.fmt("%s/samples/range/05-tuned.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&win, path.as_string()) == STATUS_OK);
    }

MTEST_END




