/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of spike-bender
 * Created on: 29 мар. 2023 г.
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

namespace
{
    typedef struct weigth_t
    {
        spike_bender::weightening_t type;
        const char *name;
    } weight_t;

    static const weight_t weight_items[] =
    {
        { spike_bender::NO_WEIGHT, "rms" },
        { spike_bender::A_WEIGHT,  "a-rms" },
        { spike_bender::B_WEIGHT,  "b-rms" },
        { spike_bender::C_WEIGHT,  "c-rms" },
        { spike_bender::D_WEIGHT,  "d-rms" },
        { spike_bender::K_WEIGHT,  "k-rms" },
    };
};

MTEST_BEGIN("spike_bender", rms)

    MTEST_MAIN
    {
        lsp::dspu::Sample ins, outs;
        io::Path in, out;

        // Load the source audio file
        MTEST_ASSERT(in.fmt("%s/samples/in/test.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::load_audio_file(&ins, in.as_string(), SAMPLE_RATE) == STATUS_OK);

        size_t period = dspu::millis_to_samples(SAMPLE_RATE, 400.0f);

        for (size_t i=0, n=sizeof(weight_items)/sizeof(weight_items[0]); i<n; ++i)
        {
            const weight_t *w = &weight_items[i];

            // Estimate the RMS and store to output file
            MTEST_ASSERT(spike_bender::estimate_rms(&outs, &ins, w->type, period) == STATUS_OK);

            MTEST_ASSERT(out.fmt("%s/samples/out/test-%s.wav", resources(), w->name) > 0);
            MTEST_ASSERT(spike_bender::save_audio_file(&outs, out.as_string()) == STATUS_OK);
        }
    }

MTEST_END


