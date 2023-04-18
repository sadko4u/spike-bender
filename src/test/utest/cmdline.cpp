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

#include <lsp-plug.in/test-fw/utest.h>
#include <lsp-plug.in/test-fw/helpers.h>
#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/lltl/parray.h>
#include <lsp-plug.in/io/Path.h>
#include <lsp-plug.in/dsp-units/filters/common.h>

#include <private/config.h>
#include <private/cmdline.h>

UTEST_BEGIN("spike_bender", cmdline)

    void validate_config(spike_bender::config_t *cfg)
    {
        LSPString key;

        // Validate root parameters
        UTEST_ASSERT(cfg->nSampleRate == 88200);
        UTEST_ASSERT(cfg->sInFile.equals_ascii("in-file.wav"));
        UTEST_ASSERT(cfg->sOutFile.equals_ascii("out-file.wav"));
        UTEST_ASSERT(float_equals_adaptive(cfg->fRange, 8.0f));
        UTEST_ASSERT(float_equals_adaptive(cfg->fKnee, 1.0f));
        UTEST_ASSERT(cfg->nPasses == 2);
        UTEST_ASSERT(cfg->enWeightening == spike_bender::A_WEIGHT);
    }

    void parse_cmdline(spike_bender::config_t *cfg)
    {
        static const char *ext_argv[] =
        {
            "-sr",  "88200",
            "-if",  "in-file.wav",
            "-of",  "out-file.wav",
            "-dr",  "8",
            "-k",   "1",
            "-np",  "2",
            "-r",   "5",
            "-wf",  "A",

            NULL
        };

        lltl::parray<char> argv;
        UTEST_ASSERT(argv.add(const_cast<char *>(full_name())));
        for (const char **pv = ext_argv; *pv != NULL; ++pv)
        {
            UTEST_ASSERT(argv.add(const_cast<char *>(*pv)));
        }

        status_t res = spike_bender::parse_cmdline(cfg, argv.size(), const_cast<const char **>(argv.array()));
        UTEST_ASSERT(res == STATUS_OK);
    }

    UTEST_MAIN
    {
        // Parse configuration from file and cmdline
        spike_bender::config_t cfg;
        parse_cmdline(&cfg);

        // Validate the final configuration
        validate_config(&cfg);
    }

UTEST_END



