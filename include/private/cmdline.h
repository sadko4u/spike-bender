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

#ifndef PRIVATE_CMDLINE_H_
#define PRIVATE_CMDLINE_H_

#include <lsp-plug.in/common/status.h>

#include <private/config.h>

namespace spike_bender
{
    using namespace lsp;

    /**
     * Output usage of the tool
     *
     * @param name command line tool name
     * @param fail return fail status code
     * @return status code
     */
    status_t print_usage(const char *name, bool fail);

    /**
     * Parse command line
     * @param cfg configuration
     * @param argc number of command line arguments
     * @param argv command line arguments
     * @return status of operation
     */
    status_t parse_cmdline(config_t *cfg, int argc, const char **argv);

} /* namespace spike_bender */


#endif /* PRIVATE_CMDLINE_H_ */
