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

MTEST_BEGIN("spike_bender", norming)

    MTEST_MAIN
    {
        lsp::dspu::Sample in, rms, env, gain;
        ssize_t rms_period, env_period;
        io::Path path;

        // STEP 0: Load the source audio file
        MTEST_ASSERT(path.fmt("%s/samples/in/test.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::load_audio_file(&in, path.as_string(), SAMPLE_RATE) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/norm/00-source.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&in, path.as_string()) == STATUS_OK);

        //----------------------------------------------------------------------------
        // ROUND 1
        // STEP 1: Estimate the envelope RMS
        rms_period = ssize_t(dspu::millis_to_samples(SAMPLE_RATE, 800.0f));
        MTEST_ASSERT(spike_bender::estimate_rms(&rms, &in, spike_bender::K_WEIGHT, rms_period*2 + 1) == STATUS_OK);
        MTEST_ASSERT(rms.remove(0, rms_period / 2) == STATUS_OK);
        rms.set_length(in.length());

        MTEST_ASSERT(path.fmt("%s/samples/norm/01-rms-1600.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&rms, path.as_string()) == STATUS_OK);

        // STEP 2: Estimate the long-tme RMS
        env_period = ssize_t(dspu::millis_to_samples(SAMPLE_RATE, 200.0f));
        MTEST_ASSERT(spike_bender::estimate_rms(&env, &in, spike_bender::K_WEIGHT, env_period*2 + 1) == STATUS_OK);
        MTEST_ASSERT(env.remove(0, env_period / 2) == STATUS_OK);
        env.set_length(in.length());

        MTEST_ASSERT(path.fmt("%s/samples/norm/02-rms-400.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&env, path.as_string()) == STATUS_OK);

        // STEP 3: Compute gain adjustment
        MTEST_ASSERT(spike_bender::calc_gain_adjust(&gain, &rms, &env) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/norm/03-gain-correction.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&gain, path.as_string()) == STATUS_OK);

        // STEP 4: Apply gain to the original sample
        MTEST_ASSERT(spike_bender::apply_gain(&in, &in, &gain) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/norm/04-in-corrected.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&in, path.as_string()) == STATUS_OK);

        // STEP 5: Apply gain to the envelope
        MTEST_ASSERT(spike_bender::apply_gain(&rms, &env, &gain) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/norm/05-env-corrected.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&rms, path.as_string()) == STATUS_OK);

        //----------------------------------------------------------------------------
        // ROUND 2
        // STEP 1: estimate the long-time RMS
        rms_period = ssize_t(dspu::millis_to_samples(SAMPLE_RATE, 200.0f));
        MTEST_ASSERT(spike_bender::estimate_rms(&rms, &in, spike_bender::K_WEIGHT, rms_period*2 + 1) == STATUS_OK);
        MTEST_ASSERT(rms.remove(0, rms_period / 2) == STATUS_OK);
        rms.set_length(in.length());

        MTEST_ASSERT(path.fmt("%s/samples/norm/06-rms-400.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&rms, path.as_string()) == STATUS_OK);

        // STEP 2: estimate the short-time RMS
        env_period = ssize_t(dspu::millis_to_samples(SAMPLE_RATE, 20.0f));
        MTEST_ASSERT(spike_bender::estimate_rms(&env, &in, spike_bender::K_WEIGHT, env_period*2 + 1) == STATUS_OK);
        MTEST_ASSERT(env.remove(0, env_period / 2) == STATUS_OK);
        env.set_length(in.length());

        MTEST_ASSERT(path.fmt("%s/samples/norm/07-rms-40.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&env, path.as_string()) == STATUS_OK);

        // STEP 2: Compute gain adjustment
        MTEST_ASSERT(spike_bender::calc_gain_adjust(&gain, &rms, &env) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/norm/08-gain-correction.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&gain, path.as_string()) == STATUS_OK);

        // STEP 3: Apply gain to the original sample
        MTEST_ASSERT(spike_bender::apply_gain(&in, &in, &gain) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/norm/09-in-corrected.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&in, path.as_string()) == STATUS_OK);

        // STEP 4: Apply gain to the envelope
        MTEST_ASSERT(spike_bender::apply_gain(&rms, &env, &gain) == STATUS_OK);

        MTEST_ASSERT(path.fmt("%s/samples/norm/10-env-corrected.wav", resources()) > 0);
        MTEST_ASSERT(spike_bender::save_audio_file(&rms, path.as_string()) == STATUS_OK);
    }

MTEST_END




