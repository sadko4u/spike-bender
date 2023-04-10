/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of spike-bender
 * Created on: 6 апр. 2023 г.
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

MTEST_BEGIN("spike_bender", vcontrol)

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

        lsp::dspu::Sample in, win, rms, avg, out, gain;
        io::Path path;
        float *rms_avg;
        size_t period;

        //----------------------------------------------------------------------------
        // ROUND 0
        // STEP 0: Load the source audio file
        MTEST_ASSERT(path.fmt("%s/samples/in/test.wav", resources()) > 0);
        MTEST_ASSERT(load_audio_file(&in, path.as_string(), SAMPLE_RATE) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/vcontrol/00-source.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&in, path.as_string()) == STATUS_OK);

        // STEP 1: Apply weighting function
        MTEST_ASSERT(spike_bender::apply_weight(&win, &in, spike_bender::K_WEIGHT) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/vcontrol/01-weighted.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&win, path.as_string()) == STATUS_OK);

        // STEP 2: Estmate average RMS
        period      = size_t(dspu::millis_to_samples(SAMPLE_RATE, 400.0f)) | 1;
        MTEST_ASSERT(spike_bender::estimate_rms(&rms, &win, spike_bender::K_WEIGHT, period) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/vcontrol/02-rms-long.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&rms, path.as_string()) == STATUS_OK);
        MTEST_ASSERT(rms_avg = new float[rms.length()]);
        for (size_t i=0; i<rms.channels(); ++i)
        {
            rms_avg[i]      = dsp::abs_max(rms.channel(i), rms.length());
            printf("RMS avg[%d] = %f (%f dB)\n", int(i), rms_avg[i], dspu::gain_to_db(rms_avg[i]));
        }

        // STEP 3: Estmate weighted RMS
        period      = size_t(dspu::millis_to_samples(SAMPLE_RATE, 40.0f)) | 1;
        MTEST_ASSERT(spike_bender::estimate_rms(&rms, &win, spike_bender::K_WEIGHT, period) == STATUS_OK);
        MTEST_ASSERT(rms.remove(0, period / 2) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/vcontrol/03-rms.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&rms, path.as_string()) == STATUS_OK);

        // STEP 3: Estmate average value
        period      = size_t(dspu::millis_to_samples(SAMPLE_RATE, 40.0f)) | 1;
        MTEST_ASSERT(spike_bender::estimate_average(&avg, &win, spike_bender::K_WEIGHT, period) == STATUS_OK);
        MTEST_ASSERT(avg.remove(0, period / 2) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/vcontrol/04-avg.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&avg, path.as_string()) == STATUS_OK);

        //----------------------------------------------------------------------------
        // ROUND 1
        // STEP 1: Adjust the gain
        spike_bender::adjust_gain(&out, &gain, &in, &rms, rms_avg, 6.0f, 3.0f);
        MTEST_ASSERT(path.fmt("%s/samples/vcontrol/05-gain.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&gain, path.as_string()) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/vcontrol/06-output.wav", resources()) > 0);
        MTEST_ASSERT(save_audio_file(&out, path.as_string()) == STATUS_OK);
    }

MTEST_END



