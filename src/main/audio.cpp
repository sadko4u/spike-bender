/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of spike-bender
 * Created on: 28 мар. 2023 г.
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

#include <lsp-plug.in/dsp-units/filters/Filter.h>
#include <lsp-plug.in/stdlib/math.h>
#include <lsp-plug.in/stdlib/stdio.h>

#include <private/audio.h>

namespace spike_bender
{
    static constexpr float PRECISION = 2.5e-8f;

    typedef struct duration_t
    {
        size_t h;
        size_t m;
        size_t s;
        size_t ms;
    } duration_t;

    void calc_duration(duration_t *d, const dspu::Sample *sample)
    {
        uint64_t duration = (uint64_t(sample->samples()) * 1000) / sample->sample_rate();
        d->ms = duration % 1000;
        duration /= 1000;
        d->s = duration % 60;
        duration /= 60;
        d->m = duration % 60;
        d->h = duration / 60;
    }

    status_t load_audio_file(dspu::Sample *sample, const LSPString *name, size_t srate)
    {
        status_t res;
        dspu::Sample temp;

        // Check arguments
        if (name == NULL)
        {
            fprintf(stderr, "  Input file name not specified\n");
            return STATUS_BAD_ARGUMENTS;
        }
        if (sample == NULL)
        {
            fprintf(stderr, "  Target sample not specified\n");
            return STATUS_BAD_ARGUMENTS;
        }

        // Load sample from file
        if ((res = temp.load(name)) != STATUS_OK)
        {
            fprintf(stderr, "  could not read file '%s', error code: %d\n", name->get_native(), int(res));
            return res;
        }

        duration_t d;
        calc_duration(&d, &temp);
        fprintf(stdout, "  loaded file: '%s', channels: %d, samples: %d, sample rate: %d, duration: %02d:%02d:%02d.%03d\n",
            name->get_native(),
            int(temp.channels()), int(temp.length()), int(temp.sample_rate()),
            int(d.h), int(d.m), int(d.s), int(d.ms));

        // Resample audio data
        if ((res = temp.resample(srate)) != STATUS_OK)
        {
            fprintf(stderr, "  could not resample file '%s' to sample rate %d, error code: %d\n",
                name->get_native(), int(srate), int(res));
            return res;
        }

        // Return result
        temp.swap(sample);

        return STATUS_OK;
    }

    status_t save_audio_file(const dspu::Sample *sample, const LSPString *name)
    {
        status_t res;

        // Check arguments
        if (name == NULL)
        {
            fprintf(stderr, "  Output file name not specified\n");
            return STATUS_BAD_ARGUMENTS;
        }
        if (sample == NULL)
        {
            fprintf(stderr, "  Input sample not specified\n");
            return STATUS_BAD_ARGUMENTS;
        }

        // Load sample from file
        if ((res = sample->save(name)) < 0)
        {
            fprintf(stderr, "  could not write file '%s', error code: %d\n", name->get_native(), int(-res));
            return -res;
        }

        duration_t d;
        calc_duration(&d, sample);
        fprintf(stdout, "  saved file: '%s', channels: %d, samples: %d, sample rate: %d, duration: %02d:%02d:%02d.%03d\n",
            name->get_native(),
            int(sample->channels()), int(sample->length()), int(sample->sample_rate()),
            int(d.h), int(d.m), int(d.s), int(d.ms));

        return STATUS_OK;
    }

    status_t estimate_rms(dspu::Sample *dst, const dspu::Sample *src, weightening_t weight, size_t period)
    {
        dspu::Filter f;

        // Initialize weighting filter
        if (!f.init(NULL))
        {
            fprintf(stderr, "  error initializing filter\n");
            return STATUS_NO_MEM;
        }

        dspu::filter_params_t fp;
        switch (weight)
        {
            case A_WEIGHT:
                fp.nType        = dspu::FLT_A_WEIGHTED;
                break;
            case B_WEIGHT:
                fp.nType        = dspu::FLT_B_WEIGHTED;
                break;
            case C_WEIGHT:
                fp.nType        = dspu::FLT_C_WEIGHTED;
                break;
            case D_WEIGHT:
                fp.nType        = dspu::FLT_D_WEIGHTED;
                break;
            case K_WEIGHT:
                fp.nType        = dspu::FLT_K_WEIGHTED;
                break;
            default:
                fp.nType        = dspu::FLT_NONE;
                break;
        }

        fp.fFreq        = 1.0f;
        fp.fFreq2       = 1.0f;
        fp.fGain        = 1.0f;
        fp.fQuality     = 0.0f;
        fp.nSlope       = 1.0f;

        f.update(src->sample_rate(), &fp);

        // Process input data with the weighting filter and compute RMS
        dspu::Sample tmp, out;
        size_t slength  = src->length();
        size_t dlength  = slength + period;
        if (!tmp.init(src->channels(), dlength, dlength))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }
        if (!out.init(src->channels(), dlength, dlength))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }

        float kperiod = 1.0f / period;

        for (size_t i=0, n=src->channels(); i<n; ++i)
        {
            const float *sbuf = src->channel(i);
            float *dbuf = tmp.channel(i);

            // Apply filter to the input buffer
            f.clear();
            f.process(dbuf, sbuf, slength);
            dsp::fill_zero(&dbuf[slength], period);
            f.process(&dbuf[slength], &dbuf[slength], period);

            // Compute the RMS value among the buffer
            float rms   = 0.0f;
            sbuf        = dbuf;
            dbuf        = out.channel(i);
            for (size_t j=0; j<dlength; ++j)
            {
                // Subtract the old value
                ssize_t off = j - period;
                if (off >= 0)
                    rms    -= sbuf[off] * sbuf[off];
                rms    += sbuf[j] * sbuf[j];
                dbuf[j]     = sqrtf(lsp_max(rms, 0.0f) * kperiod);
            }
        }

        // Return the value
        out.set_sample_rate(src->sample_rate());
        out.swap(dst);

        return STATUS_OK;
    }

    status_t calc_deviation(dspu::Sample *dst, const dspu::Sample *src, const dspu::Sample *rms, ssize_t offset)
    {
        status_t res;
        dspu::Sample out;

        if (rms->channels() != src->channels())
        {
            fprintf(stderr, "  input samples do not match by number of channels\n");
            return STATUS_BAD_ARGUMENTS;
        }

        if ((res = out.copy(src)) != STATUS_OK)
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }

        for (size_t i=0, n=out.channels(); i<n; ++i)
        {
            // Compute absolute maximum
            float *dbuf         = out.channel(i);
            const float *sbuf   = rms->channel(i);
            dsp::abs1(dbuf, out.length());

            // Compute the rectified difference between the peak value and RMS
            ssize_t head        = lsp_max(offset, 0);
            ssize_t tail        = lsp_min(rms->length() + offset, out.length());
            for (ssize_t i=head; i<tail; ++i)
                dbuf[i]     = lsp_max(dbuf[i] - sbuf[i - offset], 0.0f);
        }

        // Return the value
        out.set_sample_rate(src->sample_rate());
        out.swap(dst);

        return STATUS_OK;
    }

    status_t calc_gain_adjust(dspu::Sample *dst, const dspu::Sample *ref, const dspu::Sample *src)
    {
        dspu::Sample out;

        if (ref->channels() != src->channels())
        {
            fprintf(stderr, "  input samples do not match by number of channels\n");
            return STATUS_BAD_ARGUMENTS;
        }

        size_t count = lsp_min(ref->length(), src->length());
        if (!out.init(src->channels(), count, count))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }

        // Compute the gain relation between source sample and reference one
        for (size_t i=0; i<src->channels(); ++i)
        {
            float *dst = out.channel(i);
            const float *vref = ref->channel(i);
            const float *vsrc = src->channel(i);

            for (size_t i=0; i<count; ++i)
            {
                float aref  = fabs(vref[i]);
                float asrc  = fabs(vsrc[i]);

                dst[i]      = (asrc <= PRECISION) ? 1.0f : aref / asrc;
            }
        }

        // Return the value
        out.set_sample_rate(src->sample_rate());
        out.swap(dst);

        return STATUS_OK;
    }

    status_t apply_gain(dspu::Sample *dst, const dspu::Sample *src, const dspu::Sample *gain)
    {
        dspu::Sample out;

        if (src->channels() != gain->channels())
        {
            fprintf(stderr, "  input samples do not match by number of channels\n");
            return STATUS_BAD_ARGUMENTS;
        }

        size_t count = lsp_min(gain->length(), src->length());
        if (!out.init(src->channels(), count, count))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }

        // Compute the gain relation between source sample and reference one
        for (size_t i=0; i<src->channels(); ++i)
        {
            float *dst          = out.channel(i);
            const float *vsrc   = src->channel(i);
            const float *vgain  = gain->channel(i);

            dsp::mul3(dst, vsrc, vgain, count);
        }

        // Return the value
        out.set_sample_rate(src->sample_rate());
        out.swap(dst);

        return STATUS_OK;
    }

} /* namespace spike_bender */


