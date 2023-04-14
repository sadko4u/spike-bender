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

    MTEST_MAIN
    {

        lsp::dspu::Sample in, rms, avg, out, gain;
        io::Path path;
        float *rms_avg;
        size_t period;
        int file_id = 0;

        //----------------------------------------------------------------------------
        // ROUND 0
        // STEP 0: Load the source audio file
        MTEST_ASSERT(path.fmt("%s/samples/in/test3.wav", resources()) > 0);
        MTEST_ASSERT(load_audio_file(&in, path.as_string(), SAMPLE_RATE) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/vcontrol/%02d-source.wav", resources(), file_id++) > 0);
        MTEST_ASSERT(save_audio_file(&in, path.as_string()) == STATUS_OK);

        // STEP 1: Estmate average RMS
        period      = size_t(dspu::millis_to_samples(SAMPLE_RATE, 400.0f)) | 1;
        MTEST_ASSERT(spike_bender::estimate_rms(&rms, &in, spike_bender::K_WEIGHT, period) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/vcontrol/%02d-rms-long.wav", resources(), file_id++) > 0);
        MTEST_ASSERT(save_audio_file(&rms, path.as_string()) == STATUS_OK);
        MTEST_ASSERT(rms_avg = new float[rms.length()]);
        for (size_t i=0; i<rms.channels(); ++i)
        {
            rms_avg[i]      = dsp::abs_max(rms.channel(i), rms.length());
            printf("RMS avg[%d] = %f (%f dB)\n", int(i), rms_avg[i], dspu::gain_to_db(rms_avg[i]));
        }

        //----------------------------------------------------------------------------
        // ROUND 1
        for (int pass=0; pass<3; ++pass)
        {
            // STEP 1: Estmate weighted RMS
            period      = size_t(dspu::millis_to_samples(SAMPLE_RATE, 40.0f)) | 1;
            MTEST_ASSERT(spike_bender::estimate_rms(&rms, &in, spike_bender::K_WEIGHT, period) == STATUS_OK);
            MTEST_ASSERT(rms.remove(0, period / 2) == STATUS_OK);
            MTEST_ASSERT(path.fmt("%s/samples/vcontrol/%02d-rms-%d.wav", resources(), file_id++, pass) > 0);
            MTEST_ASSERT(save_audio_file(&rms, path.as_string()) == STATUS_OK);

            // STEP 2: Estmate average value
            period      = size_t(dspu::millis_to_samples(SAMPLE_RATE, 40.0f)) | 1;
            MTEST_ASSERT(spike_bender::estimate_average(&avg, &in, spike_bender::K_WEIGHT, period) == STATUS_OK);
            MTEST_ASSERT(avg.remove(0, period / 2) == STATUS_OK);
            MTEST_ASSERT(path.fmt("%s/samples/vcontrol/%02d-avg-%d.wav", resources(), file_id++, pass) > 0);
            MTEST_ASSERT(save_audio_file(&avg, path.as_string()) == STATUS_OK);

            // STEP 3: Adjust the gain
            spike_bender::adjust_gain(&out, &gain, &in, &rms, rms_avg, 6.0f, 3.0f);
            MTEST_ASSERT(path.fmt("%s/samples/vcontrol/%02d-gain-%d.wav", resources(), file_id++, pass) > 0);
            MTEST_ASSERT(save_audio_file(&gain, path.as_string()) == STATUS_OK);
            MTEST_ASSERT(path.fmt("%s/samples/vcontrol/%02d-output-%d.wav", resources(), file_id++, pass) > 0);
            MTEST_ASSERT(save_audio_file(&out, path.as_string()) == STATUS_OK);

            // STEP 4: commit output to input
            in.swap(out);
        }
    }

MTEST_END



