/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of spike-bender
 * Created on: 24 мар. 2023 г.
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
#include <lsp-plug.in/dsp-units/sampling/Sample.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/stdlib/stdio.h>

#include <private/config.h>
#include <private/cmdline.h>
#include <private/audio.h>
#include <private/tool.h>

namespace spike_bender
{
    int main(int argc, const char **argv)
    {
        lsp::dspu::Sample in, rms, out;
        config_t cfg;

        // Parse command line
        status_t res    = parse_cmdline(&cfg, argc, argv);
        if (res != STATUS_OK)
            return (res != STATUS_SKIP) ? print_usage(argv[0], true) : STATUS_OK;

        // Load audio file
        if ((res = load_audio_file(&in, &cfg.sInFile, cfg.nSampleRate)) != STATUS_OK)
        {
            fprintf(stderr, "Error loading audio file '%s', code=%d\n", cfg.sInFile.get_native(), int(res));
            return res;
        }
        if (cfg.nSampleRate <= 0)
            cfg.nSampleRate     = in.sample_rate();

        // Estmate average RMS
        size_t period   = size_t(dspu::millis_to_samples(cfg.nSampleRate, 400.0f)) | 1;
        if ((res = estimate_rms(&rms, &in, cfg.enWeighting, period)) != STATUS_OK)
        {
            fprintf(stderr, "Error estimating long-time RMS value, code=%d\n", int(res));
            return res;
        }

        float *rms_avg = new float[rms.length()];
        if (rms_avg == NULL)
        {
            fprintf(stderr, "Not enough memory\n");
            return STATUS_NO_MEM;
        }
        lsp_finally { delete[] rms_avg; };
        for (size_t i=0; i<rms.channels(); ++i)
            rms_avg[i]      = dsp::abs_max(rms.channel(i), rms.length());

        // Do the processing
        for (ssize_t i=0; i<cfg.nPasses; ++i)
        {
            // Commit output to input
            const dspu::Sample *src = (i > 0) ? &out : &in;

            // Estmate short-time weighted RMS
            period          = size_t(dspu::millis_to_samples(cfg.nSampleRate, cfg.fReactivity)) | 1;

            if ((res = estimate_rms(&rms, src, cfg.enWeighting, period)) != STATUS_OK)
            {
                fprintf(stderr, "Error estimating short-time RMS value for pass #%d, code=%d\n",
                    int(i), int(res));
                return res;
            }
            if ((res = rms.remove(0, period / 2)) != STATUS_OK)
            {
                fprintf(stderr, "Error cutting sample for pass #%d, code=%d\n", int(i), int(res));
                return res;
            }

            // Adjust the gain
            if ((res = adjust_gain(&out, NULL, src, &rms, rms_avg, cfg.fRange, cfg.fKnee)) != STATUS_OK)
            {
                fprintf(stderr, "Error adjusting gain for pass #%d, code=%d\n", int(i), int(res));
                return res;
            }
        }

        // Smash peaks?
        if (cfg.fPeakThresh > 1.0f)
        {
            if ((res = smash_amplitude(&out, &out, cfg.fPeakThresh)) != STATUS_OK)
            {
                fprintf(stderr, "Error smashing amplitude, error code: %d\n", int(res));
                return res;
            }
        }

        // Write result
        if (!cfg.sOutFile.is_empty())
        {
            // Normalize if required
            float ngain         = dspu::db_to_gain(cfg.fNormGain);
            if ((res = normalize(&out, ngain, cfg.enNormalize)) != STATUS_OK)
            {
                fprintf(stderr, "Error normalizing output audio file, error code: %d\n", int(res));
                return res;
            }

            if ((res = save_audio_file(&out, &cfg.sOutFile)) != STATUS_OK)
            {
                fprintf(stderr, "Error saving audio file '%s', code=%d\n", cfg.sOutFile.get_native(), int(res));
                return res;
            }
        }

        return STATUS_OK;
    }
} /* namespace spike_bender */



