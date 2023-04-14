/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of spike-bender
 * Created on: 14 апр. 2023 г.
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

MTEST_BEGIN("spike_bender", fluct)

    MTEST_MAIN
    {

        lsp::dspu::Sample in, rmsb, out;
        io::Path path;
        size_t period;
        int file_id = 0;

        //----------------------------------------------------------------------------
        // ROUND 0
        // STEP 0: Load the source audio file
        MTEST_ASSERT(path.fmt("%s/samples/in/test2.wav", resources()) > 0);
        MTEST_ASSERT(load_audio_file(&in, path.as_string(), SAMPLE_RATE) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/fluct/%02d-source.wav", resources(), file_id++) > 0);
        MTEST_ASSERT(save_audio_file(&in, path.as_string()) == STATUS_OK);

        // STEP 1: Estmate RMS values and balance between them
        period      = size_t(dspu::millis_to_samples(SAMPLE_RATE, 10.0f)) | 1;
        MTEST_ASSERT(spike_bender::estimate_rms_balance(&rmsb, &in, spike_bender::K_WEIGHT, period) == STATUS_OK);
        MTEST_ASSERT(rmsb.remove(0, period / 2) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/fluct/%02d-rms-balance.wav", resources(), file_id++) > 0);
        MTEST_ASSERT(save_audio_file(&rmsb, path.as_string()) == STATUS_OK);

        // STEP 3: Apply RMS balance
        MTEST_ASSERT(spike_bender::apply_rms_balance(&out, &in, &rmsb) == STATUS_OK);
        MTEST_ASSERT(path.fmt("%s/samples/fluct/%02d-output.wav", resources(), file_id++) > 0);
        MTEST_ASSERT(save_audio_file(&out, path.as_string()) == STATUS_OK);
    }

MTEST_END






