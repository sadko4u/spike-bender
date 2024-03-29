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

#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp-units/filters/Filter.h>
#include <lsp-plug.in/dsp-units/dynamics/DynamicProcessor.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/lltl/darray.h>
#include <lsp-plug.in/lltl/parray.h>
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

    status_t load_audio_file(dspu::Sample *sample, const LSPString *name, ssize_t srate)
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
        if (srate > 0)
        {
            if ((res = temp.resample(srate)) != STATUS_OK)
            {
                fprintf(stderr, "  could not resample file '%s' to sample rate %d, error code: %d\n",
                    name->get_native(), int(srate), int(res));
                return res;
            }
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

    status_t apply_weight(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight)
    {
        status_t res;
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
        dspu::Sample out;
        if ((res = out.copy(src)) != STATUS_OK)
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }

        for (size_t i=0, n=out.channels(); i<n; ++i)
        {
            float *dbuf = out.channel(i);

            // Apply filter to the input buffer
            f.clear();
            f.process(dbuf, dbuf, out.length());
        }

        // Return the value
        out.set_sample_rate(src->sample_rate());
        out.swap(dst);

        return STATUS_OK;
    }

    status_t estimate_rms(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period)
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

    status_t estimate_rms_balance(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period)
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
        if (!out.init(src->channels() * 5, dlength, dlength))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }

        float kperiod = 1.0f / period;
        size_t channel_id=0;

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
            float prms      = 0.0f;
            float nrms      = 0.0f;
            sbuf            = dbuf;
            float *out_prms = out.channel(channel_id++);
            float *out_nrms = out.channel(channel_id++);
            float *out_ravg = out.channel(channel_id++);
            float *out_pgain= out.channel(channel_id++);
            float *out_ngain= out.channel(channel_id++);

            for (size_t j=0; j<dlength; ++j)
            {
                // Subtract the old value
                ssize_t off = j - period;
                if (off >= 0)
                {
                    float sp= sbuf[off];
                    if (sp < 0.0f)
                        nrms       -= sp*sp;
                    else
                        prms       -= sp*sp;
                }
                float sc    = sbuf[j];
                if (sc < 0.0f)
                    nrms       += sc*sc;
                else
                    prms       += sc*sc;

                out_prms[j]  = sqrtf(lsp_max(prms, 0.0f) * kperiod);
                out_nrms[j]  = sqrtf(lsp_max(nrms, 0.0f) * kperiod);
                out_ravg[j]  = sqrtf(out_prms[j] * out_nrms[j]);
                out_pgain[j] = out_ravg[j] / out_prms[j];
                out_ngain[j] = out_ravg[j] / out_nrms[j];
            }
        }

        // Return the value
        out.set_sample_rate(src->sample_rate());
        out.swap(dst);

        return STATUS_OK;
    }

    void approximate_envelope(float *dst, const float *src, size_t count)
    {
        size_t ppeak    = 0;

        // Perform the smoothing
        for (size_t i=0; i<count; ++i)
        {
            float s     = src[i];
            if ((s == 0.0f) || (i <= ppeak))
                continue;

            dsp::smooth_cubic_linear(&dst[ppeak], src[ppeak], s, i - ppeak);
            ppeak       = i;
        }

        // Do the last smooth
        if (ppeak < count)
            dsp::smooth_cubic_linear(&dst[ppeak], src[ppeak], src[count - 1], count - ppeak - 1);
    }

    status_t estimate_envelope(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period)
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
        size_t slength = src->length();
        size_t dlength = slength + (period - slength % period) % period;
        if (!tmp.init(src->channels(), dlength, dlength))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }
        if (!out.init(src->channels() * 6, dlength, dlength))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }

        size_t channel_id=0;

        for (size_t i=0, n=src->channels(); i<n; ++i)
        {
            const float *sbuf   = src->channel(i);
            float *tbuf         = tmp.channel(i);

            // Apply filter to the input buffer
            f.clear();
            f.process(tbuf, sbuf, slength);
            dsp::fill_zero(&tbuf[slength], dlength - slength);
            f.process(&tbuf[slength], &tbuf[slength], dlength - slength);

            // Find the maximum and minimum envelope values
            float *out_ppeak    = out.channel(channel_id++);
            float *out_npeak    = out.channel(channel_id++);
            float *out_psmooth  = out.channel(channel_id++);
            float *out_nsmooth  = out.channel(channel_id++);
            float *out_delta    = out.channel(channel_id++);
            float *out_result   = out.channel(channel_id++);

            // Step 1: find maximum values
            for (size_t j=0; j<dlength; j += period)
            {
                size_t imin         = dsp::min_index(&tbuf[j], period);
                size_t imax         = dsp::max_index(&tbuf[j], period);
                float min           = tbuf[j + imin];
                float max           = tbuf[j + imax];
                if (min < 0.0f)
                    out_npeak[j + imin]     = min;
                if (max > 0.0f)
                    out_ppeak[j + imax]     = max;
            }

            // Step 2: approximate the envelope around the values
            approximate_envelope(out_psmooth, out_ppeak, dlength);
            approximate_envelope(out_nsmooth, out_npeak, dlength);

            // Compute the average value
            dsp::lr_to_mid(out_delta, out_psmooth, out_nsmooth, dlength);

            // Apply the result
            dsp::sub3(out_result, sbuf, out_delta, lsp_min(dlength, slength));
        }

        // Return the value
        out.set_sample_rate(src->sample_rate());
        out.swap(dst);

        return STATUS_OK;
    }

    status_t apply_rms_balance(dspu::Sample *dst, const dspu::Sample *src, const dspu::Sample *rms)
    {
        // Process input data with the weighting filter and compute RMS
        dspu::Sample out;
        size_t count    = lsp_min(rms->length(), src->length());
        if (!out.init(src->channels(), count, count))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }

        size_t channel_id=0;

        for (size_t i=0, n=src->channels(); i<n; ++i, channel_id += 5)
        {
            const float *sbuf  = src->channel(i);
            const float *pgain = rms->channel(channel_id + 3);
            const float *ngain = rms->channel(channel_id + 4);
            float *dbuf = out.channel(i);

            // Compute the RMS value among the buffer
            for (size_t j=0; j<count; ++j)
            {
                float s             = sbuf[j];
                dbuf[j]             = (s < 0.0f) ? s * pgain[j] * M_SQRT2 : s * ngain[j] * M_SQRT2;
            }
        }

        // Return the value
        out.set_sample_rate(src->sample_rate());
        out.swap(dst);

        return STATUS_OK;
    }

    status_t estimate_partial_rms(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period, bool positive)
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
                {
                    float sp= (positive) ? lsp_max(sbuf[off], 0.0f) : -lsp_min(sbuf[off], 0.0f);
                    rms    -= sp * sp;
                }
                float sc    = (positive) ? lsp_max(sbuf[j], 0.0f) : -lsp_min(sbuf[j], 0.0f);
                rms        += sc * sc;
                dbuf[j]     = sqrtf(lsp_max(rms, 0.0f) * kperiod);
            }
        }

        // Return the value
        out.set_sample_rate(src->sample_rate());
        out.swap(dst);

        return STATUS_OK;
    }

    status_t estimate_average(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period)
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
            float avg   = 0.0f;
            sbuf        = dbuf;
            dbuf        = out.channel(i);
            for (size_t j=0; j<dlength; ++j)
            {
                // Subtract the old value
                ssize_t off = j - period;
                if (off >= 0)
                    avg    -= sbuf[off];
                avg        += sbuf[j];
                dbuf[j]     = avg * kperiod;
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

    status_t find_peaks(lltl::darray<range_t> *ranges, const float *buf, const float *rms, float threshold, size_t count)
    {
        lltl::darray<range_t> out;
        range_t *curr       = NULL;
        float s_prev        = 0.0f;     // Previous sample
        float d_prev        = 0.0f;     // Previous derivative
        ssize_t num_flips   = 0;        // Number of flips for current region
        ssize_t last_flip   = -1;       // Last sign flip

        if ((curr = out.add()) == NULL)
            return STATUS_NO_MEM;
        curr->first     = 0;
        curr->last      = curr->first;
        curr->peak      = curr->first;
        curr->gain      = 0.0f;

        for (size_t i=0; i<count; ++i)
        {
            float s     = buf[i];
            float d     = buf[i] - s_prev;

            // If the sign of the derivative has changed, then we have local minimum or maximum
            if (((d_prev < 0.0f) && (d >= 0.0f)) ||
                ((d_prev > 0.0f) && (d <= 0.0f)))
            {
                float gain  = fabsf(buf[i-1]);
//                lsp_trace("peak %.6f at %d", gain, int(i));
                if (curr->gain < gain)
                {
                    curr->gain      = gain;
                    curr->peak      = i-1;
                }
            }

            // If the sign of the sample has changed, then we reached potential end of region
            if (((s_prev < 0.0f) && (s >= 0.0f)) ||
                ((s_prev > 0.0f) && (s <= 0.0f)))
            {
                // Increment number of flips
                ++num_flips;

                float thresh    = lsp_max(rms[curr->peak] * M_SQRT1_2, threshold);

                // Trigger region cut only if the peak is greater than the threshold
                if (curr->gain >= thresh)
                {
                    if (num_flips > 1)
                    {
                        // There was data before the last peak, need to cut two regions
                        float gain      = curr->gain;
                        size_t peak     = curr->peak;

                        curr->last      = last_flip;
                        curr->peak      = curr->first + dsp::abs_max_index(&buf[curr->first], curr->last - curr->first);
                        curr->gain      = fabs(buf[curr->peak]);

                        if ((curr = out.add()) == NULL)
                            return STATUS_NO_MEM;

                        curr->first     = last_flip;
                        curr->last      = curr->first;
                        curr->peak      = peak;
                        curr->gain      = gain;
                    }

                    // This is the first flip in the range with the peak after the threshold
                    curr->last      = i;

                    // Add new range
                    if ((curr = out.add()) == NULL)
                        return STATUS_NO_MEM;

                    curr->first     = last_flip;
                    curr->last      = curr->first;
                    curr->peak      = curr->first;
                    curr->gain      = 0.0f;

                    // Reset number of flips
                    num_flips       = 0;
                }

                // Update index of the last flip
                last_flip   = i;
            }

            // Update previous sample and derivative
            s_prev      = s;
            d_prev      = d;
        }

        // Determine what to do with the last region
        if (curr->first >= count)
            out.pop();
        else
            curr->last      = count;

        // Return result
        out.swap(ranges);

        return STATUS_OK;
    }

    status_t apply_gain(float *buf, lltl::darray<range_t> *ranges, float threshold)
    {
        for (size_t i=0, n=ranges->size(); i<n; ++i)
        {
            range_t *r  = ranges->uget(i);
            if (r->gain < threshold)
                continue;

//            lsp_trace("-----");
//            for (size_t i=r->first; i<r->last; ++i)
//                lsp_trace("buf[%d] = %.6f", int(i), buf[i]);

            float gain  = dsp::abs_max(&buf[r->first], r->last - r->first);

            dsp::mul_k2(&buf[r->first], 1.0f / gain, r->last - r->first);
        }

        return STATUS_OK;
    }

    status_t adjust_gain(
        dspu::Sample *dst,
        dspu::Sample *gain,
        const dspu::Sample *src,
        const dspu::Sample *env,
        const float *thresh,
        float range_db,
        float knee_db)
    {
        dspu::Sample out, g;
        dspu::DynamicProcessor dp;

        // Check arguments
        if (src->channels() != env->channels())
        {
            fprintf(stderr, "  input samples do not match by number of channels\n");
            return STATUS_BAD_ARGUMENTS;
        }

        // Initialize the output samples
        size_t count = lsp_min(env->length(), src->length());
        if (!out.init(src->channels(), count, count))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }
        if (!g.init(src->channels(), count, count))
        {
            fprintf(stderr, "  not enough memory\n");
            return STATUS_NO_MEM;
        }

//        constexpr int N = 201;
//        float curve_in[N], curve_out[N];
//        float curve_in_db[N], curve_out_db[N];

        // Process each channel
        for (size_t i=0; i<src->channels(); ++i)
        {
            // Configure the dynamic processor
            dp.construct();
            lsp_finally { dp.destroy(); };
            dp.set_sample_rate(src->sample_rate());

            dspu::dyndot_t dot;
            dot.fOutput = thresh[i];
            dot.fKnee   = dspu::db_to_gain(-fabs(knee_db));

            dot.fInput  = thresh[i] * dspu::db_to_gain(range_db - 3.0f);
            dp.set_dot(0, &dot);

//            lsp_trace("dot[0] x=%f, y=%f", dspu::gain_to_db(dot.fInput), dspu::gain_to_db(dot.fOutput));

            dot.fInput  = thresh[i] * dspu::db_to_gain(-range_db - 3.0f);
            dp.set_dot(1, &dot);

//            lsp_trace("dot[1] x=%f, y=%f", dspu::gain_to_db(dot.fInput), dspu::gain_to_db(dot.fOutput));

            dot.fInput  = -1.0f;
            dp.set_dot(2, &dot);
            dp.set_dot(3, &dot);

            dp.set_attack_time(0, 0.0f);
            dp.set_attack_level(0, thresh[i] * dspu::db_to_gain(-6.0f));

//            lsp_trace("attack[0] = %f", dspu::gain_to_db(dp.get_attack_level(0)));
            dp.set_attack_time(1, 5.0f);
            dp.set_attack_level(1, -1.0f);
            dp.set_attack_level(2, -1.0f);
            dp.set_attack_level(3, -1.0f);

            dp.set_release_time(0, 5.0f);
            dp.set_release_level(0, thresh[i] * dspu::db_to_gain(-6.0f));
//            lsp_trace("release[0] = %f", dspu::gain_to_db(dp.get_release_level(0)));
            dp.set_release_time(1, 2.0f);
            dp.set_release_level(1, -1.0f);
            dp.set_release_level(2, -1.0f);
            dp.set_release_level(3, -1.0f);

            dp.set_in_ratio(1.0f);
            dp.set_out_ratio(1.0f);

            dp.update_settings();

//            printf("in;out;\n");
//            for (ssize_t k=0; k<N; ++k)
//            {
//                curve_in_db[k]  = k*0.5f - 100.0f;
//                curve_in[k]     = dspu::db_to_gain(curve_in_db[k]);
//            }
//            dp.curve(curve_out, curve_in, N);
//            for (ssize_t k=0; k<N; ++k)
//            {
//                curve_out_db[k] = dspu::gain_to_db(curve_out[k]);
//                printf("%.2f;%.2f;\n", curve_in_db[k], curve_out_db[k]);
//            }

            // Perform the processing
            const float *vsrc   = src->channel(i);
            const float *venv   = env->channel(i);
            float *vgain        = g.channel(i);
            float *vdst         = out.channel(i);

            dp.process(vgain, NULL, venv, count);
            dsp::mul3(vdst, vgain, vsrc, count);
        }

        // Return result
        out.set_sample_rate(src->sample_rate());
        g.set_sample_rate(src->sample_rate());


        if (dst != NULL)
            out.swap(dst);
        if (gain != NULL)
            g.swap(gain);

        return STATUS_OK;
    }

    status_t normalize(dspu::Sample *dst, float gain, normalize_t mode)
    {
        if (mode == NORM_NONE)
            return STATUS_OK;

        float peak  = 0.0f;
        for (size_t i=0, n=dst->channels(); i<n; ++i)
        {
            float cpeak = dsp::abs_max(dst->channel(i), dst->length());
            peak        = lsp_max(peak, cpeak);
        }

        // No peak detected?
        if (peak < 1e-6)
            return STATUS_OK;

        switch (mode)
        {
            case NORM_BELOW:
                if (peak >= gain)
                    return STATUS_OK;
                break;
            case NORM_ABOVE:
                if (peak <= gain)
                    return STATUS_OK;
                break;
            default:
                break;
        }

        // Adjust gain
        float k = gain / peak;
        for (size_t i=0, n=dst->channels(); i<n; ++i)
            dsp::mul_k2(dst->channel(i), k, dst->length());

        return STATUS_OK;
    }

    static inline ssize_t compare_range(const void *a, const void *b, size_t size)
    {
        const range_t *fa = static_cast<const range_t *>(a);
        const range_t *fb = static_cast<const range_t *>(b);
        if (fa->gain < fb->gain)
            return -1;
        return (fa->gain > fb->gain) ? 1 : 0.0f;
    }

    static inline bool median_value(float *res, const lltl::darray<range_t> *list)
    {
        lltl::parray<range_t> me;

        if (!me.reserve(list->size()))
            return false;

        for (size_t i=0, n=list->size(); i<n; ++i)
        {
            if (!me.add(const_cast<range_t *>(list->uget(i))))
                return false;
        }

        me.qsort(compare_range);

        size_t size = list->size();
        if (size < 2)
        {
            *res = (size > 0) ? me.uget(0)->gain : 0.0f;
            return true;
        }
        if (size & 1)
        {
            *res = 0.5f * (me.uget(size >> 1)->gain + me.uget((size >> 1) + 1)->gain);
            return true;
        }

        *res = me.uget(size >> 1)->gain;
        return true;
    }

    static inline ssize_t compare_peak(const void *a, const void *b, size_t size)
    {
        const peak_t *fa = static_cast<const peak_t *>(a);
        const peak_t *fb = static_cast<const peak_t *>(b);
        if (fa->gain < fb->gain)
            return -1;
        return (fa->gain > fb->gain) ? 1 : 0.0f;
    }

    static inline bool median_value(float *res, const lltl::darray<peak_t> *list)
    {
        lltl::parray<peak_t> me;

        if (!me.reserve(list->size()))
            return false;

        for (size_t i=0, n=list->size(); i<n; ++i)
        {
            if (!me.add(const_cast<peak_t *>(list->uget(i))))
                return false;
        }

        me.qsort(compare_peak);

        size_t size = list->size();
        if (size < 2)
        {
            *res = (size > 0) ? me.uget(0)->gain : 0.0f;
            return true;
        }
        if (size & 1)
        {
            *res = 0.5f * (me.uget(size >> 1)->gain + me.uget((size >> 1) + 1)->gain);
            return true;
        }

        *res = me.uget(size >> 1)->gain;
        return true;
    }

    static inline bool check_threshold(const range_t *r, float pos, float neg)
    {
        if (r->gain > 0.0f)
            return r->gain >= pos;
        return r->gain <= neg;
    }

    typedef struct g_point_t
    {
        size_t  pos;
        float   gain;
    } g_point_t;

    static inline float interpolate(float a, float b, float x)
    {
        float d = b - a;
        return a + d * x * x * (3.0f - 2.0f * x);
    }

    static status_t smash_range(float *v, lltl::darray<range_t> & ranges, size_t & index, float p_avg, float n_avg)
    {
        lltl::darray<g_point_t> points;
        size_t first    = index;
        size_t last     = index;

        // Find start of the peak
        while (first > 0)
        {
            range_t *r = ranges.uget(first - 1);
            if (!check_threshold(r, p_avg, n_avg))
                break;
            --first;
        }

        // Find end of the peak
        while (last < ranges.size() - 1)
        {
            range_t *r = ranges.uget(last + 1);
            if (!check_threshold(r, p_avg, n_avg))
                break;
            ++last;
        }

        // Form the list of gain points
        g_point_t *p    = NULL;
        range_t *r      = NULL;
        r               = ranges.uget(first);
        if ((p = points.add()) == NULL)
            return STATUS_NO_MEM;
        p->pos          = r->first;
        p->gain         = 1.0f;

        for (size_t i=first; i<=last; ++i)
        {
            range_t *r      = ranges.uget(i);
            if ((p = points.add()) == NULL)
                return STATUS_NO_MEM;
            p->pos          = r->peak;
            p->gain         = (r->gain >= 0.0f) ? p_avg / r->gain : n_avg / r->gain;
        }

        r               = ranges.uget(last);
        if ((p = points.add()) == NULL)
            return STATUS_NO_MEM;
        p->pos          = r->last;
        p->gain         = 1.0f;

        // Apply gain compensation
        lsp_trace("Applying gain compensation for range[%d, %d]", int(points.first()->pos), int(points.last()->pos));

        g_point_t *pp   = points.uget(0);
        for (size_t i=1, n=points.size(); i < n; ++i)
        {
            p               = points.uget(i);
            float k         = 1.0f / (p->pos - pp->pos);

            for (size_t j=pp->pos; j<p->pos; ++j)
                v[j]           *= interpolate(pp->gain, p->gain, (j - pp->pos) * k);

            pp              = p;
        }

        // Return new index
        index           = last + 1;

        return STATUS_OK;
    }

    status_t smash_amplitude_old(dspu::Sample *dst, const dspu::Sample *src, float threshold)
    {
        lltl::darray<range_t> p_me, n_me, ranges;
        status_t res;
        dspu::Sample out;
        range_t curr;

        if ((res = out.copy(src)) != STATUS_OK)
            return res;
        out.set_sample_rate(src->sample_rate());

        curr.first      = 0;
        curr.last       = 0;
        curr.gain       = 0.0f;
        curr.peak       = 0;

        for (size_t i=0, n=src->channels(); i<n; ++i)
        {
            // Pass 1: find the median peak gain
            // Signals can be asymmetric, so we estimate average for positive and negative peaks separately
            float *in       = out.channel(i);
            float s_prev    = 0.0f;

            for (size_t j=0, m=out.samples(); j<m; ++j)
            {
                float s         = in[j];
                if (fabsf(s) > fabsf(curr.gain))
                {
                    curr.gain       = s;
                    curr.peak       = j;
                }

                // Crossing the zero?
                if (((s_prev < 0.0f) && (s >= 0.0f)) ||
                    ((s_prev > 0.0f) && (s <= 0.0f)))
                {
                    curr.last       = j;

                    // Add new range to the list
                    if (curr.last > curr.first)
                    {
                        lltl::darray<range_t> & x_me = (curr.gain > 0.0f) ? p_me : n_me;
                        if (!ranges.add(&curr))
                            return STATUS_NO_MEM;
                        if (!x_me.add(&curr))
                            return STATUS_NO_MEM;
                    }

                    curr.first      = j;
                    curr.gain       = 0.0f;
                    curr.peak       = 0;
                }

                s_prev          = s;
            }

            // Estimate median values
            float p_avg     = 0.0f;
            float n_avg     = 0.0f;
            if (!median_value(&p_avg, &p_me))
                return STATUS_NO_MEM;
            if (!median_value(&n_avg, &n_me))
                return STATUS_NO_MEM;
            p_me.clear();
            n_me.clear();

            // Pass 2: pass through the list of peaks and adjust the volume
            for (size_t j=0, m=ranges.size(); j<m; )
            {
                range_t *r      = ranges.uget(j);
                if (check_threshold(r, p_avg * threshold, n_avg * threshold))
                {
                    if ((res = smash_range(in, ranges, j, p_avg, n_avg)) != STATUS_OK)
                        return res;
                }
                else
                    ++j;
            }

            // Flush ranges
            ranges.clear();
        }

        // Commit the result
        out.swap(dst);

        return STATUS_OK;
    }

    status_t smash_amplitude_half(dspu::Sample *dst, const dspu::Sample *src, float threshold)
    {
        lltl::darray<peak_t> peaks;
        status_t res;
        dspu::Sample out;
        peak_t curr;

        if ((res = out.copy(src)) != STATUS_OK)
            return res;
        out.set_sample_rate(src->sample_rate());

        size_t step = src->sample_rate() / 100;

        for (size_t i=0, n=src->channels(); i<n; ++i)
        {
            // Pass 1: quantize peak values
            float *in       = out.channel(i);
            float s_prev    = 0.0f;
            float ds_prev   = 0.0f;
            float ds        = 0.0f;

            curr.index      = 0;
            curr.gain       = -1.0f;
            peaks.clear();

            for (size_t j=0, m=out.samples(); j<m; ++j)
            {
                // Add quantized point
                if (((j % step) == 0) && (curr.gain > 0.0f))
                {
                    if (!peaks.append(&curr))
                        return STATUS_NO_MEM;

                    curr.index = 0;
                    curr.gain  = -1.0f;
                }

                // Find another local peak
                float s         = in[j];
                ds              = s - s_prev;
                if ((ds < 0.0f) && (ds_prev >= 0.0f) && (curr.gain < s))
                {
                    curr.index      = j;
                    curr.gain       = s;
                }

                ds_prev         = ds;
                s_prev          = s;
            }

            // Pass 2: Estimate median values
            float avg       = 0.0f;
            if (!median_value(&avg, &peaks))
                return STATUS_NO_MEM;

            // Pass 3: Find all positive peaks
            s_prev          = 0.0f;
            ds_prev         = 0.0f;
            ds              = 0.0f;

            peaks.clear();

            for (size_t j=0, m=out.samples(); j<m; ++j)
            {
                // Find local peak
                float s         = in[j];
                ds              = s - s_prev;
                if ((ds < 0.0f) && (ds_prev >= 0.0f) && (s > 0.0f))
                {
                    curr.index      = j;
                    curr.gain       = s;
                    if (!peaks.add(&curr))
                        return STATUS_NO_MEM;
                }

                ds_prev         = ds;
                s_prev          = s;
            }

            // Add last peak at the end of file
            curr.index      = out.length();
            curr.gain       = 1.0f;
            if (!peaks.add(&curr))
                return STATUS_NO_MEM;

            // Pass 4: Walk through the peaks and tune them. Invert the sign of output signal
            size_t idx      = 0;
            float gain      = 1.0f;

            for (size_t j=0, m=peaks.size(); j<m; ++j)
            {
                const peak_t *p = peaks.uget(j);
                float egain     = (p->gain > threshold * avg) ? avg*threshold / p->gain : 1.0f;
                float delta     = 1.0f / float(p->index - idx);

                for (size_t k=idx; k<p->index; ++k)
                    in[k]          *= - interpolate(gain, egain, (k - idx) * delta);

                idx             = p->index;
                gain            = egain;
            }
        }

        // Commit the result
        out.swap(dst);

        return STATUS_OK;
    }

    status_t smash_amplitude(dspu::Sample *dst, const dspu::Sample *src, float threshold)
    {
        lltl::darray<peak_t> peaks, p_peaks, n_peaks;
        status_t res;
        dspu::Sample out;
        peak_t pos, neg, pk;

        if ((res = out.copy(src)) != STATUS_OK)
            return res;
        out.set_sample_rate(src->sample_rate());

        size_t step = src->sample_rate() / 100;

        for (size_t i=0, n=src->channels(); i<n; ++i)
        {
            // Pass 1: quantize peak values
            float *in       = out.channel(i);

            pos.index       = 0;
            pos.gain        = -1.0f;
            neg.index       = 0;
            neg.gain        = 1.0f;

            p_peaks.clear();
            n_peaks.clear();
            peaks.clear();

            for (size_t j=0, m=out.samples(); j<m; ++j)
            {
                // Add quantized point
                if ((j % step) == 0)
                {
                    if (pos.gain >= 0.0f)
                    {
                        if (!p_peaks.append(&pos))
                            return STATUS_NO_MEM;
                    }
                    if (neg.gain <= 0.0f)
                    {
                        if (!n_peaks.append(&neg))
                            return STATUS_NO_MEM;
                    }

                    pos.index       = 0;
                    pos.gain        = -1.0f;
                    neg.index       = 0;
                    neg.gain        = 1.0f;
                }

                // Find another local peak
                float s         = in[j];
                float s_prev    = (j > 0) ? in[j-1] : 0.0f;
                float s_next    = (j < m) ? in[j+1] : 0.0f;
                float ds_prev   = s - s_prev;
                float ds_next   = s_next - s;
                if ((ds_next < 0.0f) && (ds_prev >= 0.0f) && (s > 0.0f))
                {
                    pk.index        = j;
                    pk.gain         = s;
                    if (!peaks.add(&pk))
                        return STATUS_NO_MEM;

                    // We've just passed positive peak at the previous step
                    if (pos.gain < pk.gain)
                        pos             = pk;
                }
                else if ((ds_next > 0.0f) && (ds_prev <= 0.0f) && (s < 0.0f))
                {
                    pk.index        = j;
                    pk.gain         = s;
                    if (!peaks.add(&pk))
                        return STATUS_NO_MEM;

                    // We've just passed negative peak at the previous step
                    if (neg.gain > pk.gain)
                        neg             = pk;
                }

                s_prev          = s;
            }

            // Pass 2: Estimate median values
            float p_avg     = 0.0f;
            float n_avg     = 0.0f;
            if (!median_value(&p_avg, &p_peaks))
                return STATUS_NO_MEM;
            if (!median_value(&n_avg, &n_peaks))
                return STATUS_NO_MEM;

            // Add last peak at the end of file
            pk.index        = out.length();
            pk.gain         = 1.0f;
            if (!peaks.add(&pk))
                return STATUS_NO_MEM;

            // Pass 3: Walk through the peaks and tune them. Invert the sign of output signal
            size_t idx      = 0;
            float gain      = 1.0f;

            for (size_t j=0, m=peaks.size(); j<m; ++j)
            {
                const peak_t *p = peaks.uget(j);
                float avg       = (p->gain > 0.0f) ? p_avg : n_avg;
                float egain     = (fabsf(p->gain) > threshold * fabsf(avg)) ? avg*threshold / p->gain : 1.0f;
                float delta     = 1.0f / float(p->index - idx);

                for (size_t k=idx; k<p->index; ++k)
                    in[k]          *= interpolate(gain, egain, (k - idx) * delta);

                idx             = p->index;
                gain            = egain;
            }
        }

        // Commit the result
        out.swap(dst);

        return STATUS_OK;
    }

} /* namespace spike_bender */


