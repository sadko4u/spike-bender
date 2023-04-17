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

#include <private/config.h>

namespace spike_bender
{

    config_t::config_t()
    {
        nSampleRate         = -1;
        nPasses             = 1;
        fReactivity         = 40.0f;
        fRange              = 6.0f;
        fKnee               = 3.0f;
        enWeightening       = NO_WEIGHT;
    }

    config_t::~config_t()
    {
        clear();
    }

    void config_t::clear()
    {
        nSampleRate         = -1;
        nPasses             = 1;
        fReactivity         = 40.0f;
        fRange              = 6.0f;
        fKnee               = 3.0f;
        enWeightening       = NO_WEIGHT;

        sInFile.clear();
        sOutFile.clear();
    }


} /* namespace spike_bender */



