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

#ifndef PRIVATE_AUDIO_H_
#define PRIVATE_AUDIO_H_

#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/dsp-units/sampling/Sample.h>
#include <lsp-plug.in/lltl/darray.h>

namespace spike_bender
{
    using namespace lsp;

    /**
     * Frequency weightening function for the RMS estimation
     */
    enum weighting_t
    {
        NO_WEIGHT,  /**< No frequency weightening function */
        A_WEIGHT,   /**< A-Weighting filter applied */
        B_WEIGHT,   /**< B-Weighting filter applied */
        C_WEIGHT,   /**< C-Weighting filter applied */
        D_WEIGHT,   /**< D-Weighting filter applied */
        K_WEIGHT    /**< K-Weighting filter applied */
    };

    enum normalize_t
    {
        NORM_NONE,      // No normalization
        NORM_ABOVE,     // When the maximum peak is above the threshold
        NORM_BELOW,     // When the maximum peak is below the threshold
        NORM_ALWAYS     // Always normalize
    };

    typedef struct range_t
    {
        size_t first;   // The first sample in range
        size_t last;    // The first sample after the range
        size_t peak;    // The peak index
        float  gain;    // The peak value
    } range_t;

    typedef struct peak_t
    {
        size_t index;   // The peak index
        float  gain;    // The peak value
    } peak_t;

    /**
     * Load audio file and perform resampling
     *
     * @param sample sample to store audio data
     * @param name name of the file
     * @param srate desired sample rate
     * @return status of operation
     */
    status_t load_audio_file(dspu::Sample *sample, const LSPString *name, ssize_t srate);

    /**
     * Save audio file
     *
     * @param sample sample to save
     * @param name file name
     * @return status of operation
     */
    status_t save_audio_file(const dspu::Sample *sample, const LSPString *name);


    /**
     * Estimate the RMS of the input sample and store to another sample
     * @param dst destination sample to store data, is larger by period number of samples
     * @param src source sample to read data
     * @param weight weightening function
     * @param period the RMS estimation frame size in samples
     * @return status of operation
     */
    status_t apply_weight(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight);

    /**
     * Estimate the RMS of the input sample and store to another sample
     * @param dst destination sample to store data, is larger by period number of samples
     * @param src source sample to read data
     * @param weight weightening function
     * @param period the RMS estimation frame size in samples
     * @return status of operation
     */
    status_t estimate_rms(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period);
    status_t estimate_average(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period);
    status_t estimate_partial_rms(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period, bool positive);
    status_t estimate_rms_balance(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period);
    status_t apply_rms_balance(dspu::Sample *dst, const dspu::Sample *src, const dspu::Sample *rms);

    /**
     * Compute the deviation above the RMS value for the sample
     * @param dst destination sample to store the value
     * @param src the original sample
     * @param rms the RMS of the original sample
     * @param offset the offset of the sample relative to the RMS value
     * @return status of operation
     */
    status_t calc_deviation(dspu::Sample *dst, const dspu::Sample *src, const dspu::Sample *rms, ssize_t offset);

    /**
     * Compute gain adjustment
     * @param dst destination sample to store result
     * @param ref reference gain
     * @param src source gain
     * @return status of operation
     */
    status_t calc_gain_adjust(dspu::Sample *dst, const dspu::Sample *ref, const dspu::Sample *src);

    status_t apply_gain(dspu::Sample *dst, const dspu::Sample *src, const dspu::Sample *gain);


    status_t find_peaks(lltl::darray<range_t> *ranges, const float *buf, const float *rms, float threshold, size_t count);

    status_t apply_gain(float *buf, lltl::darray<range_t> *ranges, float threshold);

    status_t adjust_gain(
        dspu::Sample *dst,
        dspu::Sample *gain,
        const dspu::Sample *src,
        const dspu::Sample *env,
        const float *thresh,
        float range_db,
        float knee_db);

    status_t estimate_envelope(dspu::Sample *dst, const dspu::Sample *src, weighting_t weight, size_t period);

    status_t smash_amplitude(dspu::Sample *dst, const dspu::Sample *src, float threshold);

    /**
     * Normalize sample to the specified gain
     * @param dst sample to normalize
     * @param gain the maximum peak gain
     * @param mode the normalization mode
     * @return status of operation
     */
    status_t normalize(dspu::Sample *dst, float gain, normalize_t mode);
} /* namespace spike_bender */



#endif /* PRIVATE_AUDIO_H_ */
