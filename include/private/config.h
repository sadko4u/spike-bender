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

#ifndef PRIVATE_CONFIG_H_
#define PRIVATE_CONFIG_H_

#include <lsp-plug.in/runtime/LSPString.h>
#include <private/audio.h>

namespace spike_bender
{
    using namespace lsp;

    /**
     * Overall configuration
     */
    struct config_t
    {
        private:
            config_t & operator = (const config_t &);

        public:
            ssize_t                                 nSampleRate;    // Sample rate
            LSPString                               sInFile;        // Input file
            LSPString                               sOutFile;       // Output file
            ssize_t                                 nPasses;        // Number of passes
            float                                   fReactivity;    // Reactivity in milliseconds
            float                                   fRange;         // Range in decibels
            float                                   fKnee;          // Knee in decibels
            weighting_t                             enWeighting;    // Weighting function
            normalize_t                             enNormalize;    // Normalization method
            float                                   fNormGain;      // Normalization gain
            float                                   fPeakThresh;    // Amplitude smash threshold
            bool                                    bEliminatePeaks;// Eliminate peaks

        public:
            explicit config_t();
            ~config_t();

        public:
            void clear();
    };

} /* namespace spike_bender */


#endif /* PRIVATE_CONFIG_H_ */
