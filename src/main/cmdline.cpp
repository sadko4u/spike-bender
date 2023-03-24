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

#include <lsp-plug.in/lltl/pphash.h>
#include <lsp-plug.in/stdlib/string.h>
#include <lsp-plug.in/stdlib/stdio.h>
#include <lsp-plug.in/io/InStringSequence.h>
#include <lsp-plug.in/expr/Tokenizer.h>

#include <private/config.h>
#include <private/cmdline.h>

namespace spike_bender
{
    using namespace lsp;

    typedef struct option_t
    {
        const char *s_short;
        const char *s_long;
        bool        s_flag;
        const char *s_desc;
    } option_t;

    typedef struct cfg_flag_t
    {
        const char     *name;
        ssize_t         value;
    } cfg_flag_t;

    static const option_t options[] =
    {
        { "-if",  "--in-file",          false,     "Input file"                                             },
        { "-of",  "--out-file",         false,     "Output file"                                            },
        { "-sr",  "--srate",            false,     "Sample rate of output file"                             },

        { NULL, NULL, false, NULL }
    };

    status_t print_usage(const char *name, bool fail)
    {
        LSPString buf, fmt;
        size_t maxlen = 0;

        // Estimate maximum parameter size
        for (const option_t *p = spike_bender::options; p->s_short != NULL; ++p)
        {
            buf.fmt_ascii("%s, %s", p->s_short, p->s_long);
            maxlen  = lsp_max(buf.length(), maxlen);
        }
        fmt.fmt_ascii("  %%-%ds    %%s\n", int(maxlen));

        // Output usage
        printf("usage: %s [arguments]\n", name);
        printf("available arguments:\n");
        for (const option_t *p = spike_bender::options; p->s_short != NULL; ++p)
        {
            buf.fmt_ascii("%s, %s", p->s_short, p->s_long);
            printf(fmt.get_native(), buf.get_native(), p->s_desc);
        }

        return (fail) ? STATUS_BAD_ARGUMENTS : STATUS_SKIP;
    }

    const cfg_flag_t *find_config_flag(const LSPString *s, const cfg_flag_t *flags)
    {
        for (size_t i=0; (flags != NULL) && (flags->name != NULL); ++i, ++flags)
        {
            if (s->equals_ascii_nocase(flags->name))
                return flags;
        }
        return NULL;
    }

    status_t parse_cmdline_int(ssize_t *dst, const char *val, const char *parameter)
    {
        LSPString in;
        if (!in.set_native(val))
            return STATUS_NO_MEM;

        io::InStringSequence is(&in);
        expr::Tokenizer t(&is);
        ssize_t ivalue;

        switch (t.get_token(expr::TF_GET))
        {
            case expr::TT_IVALUE: ivalue = t.int_value(); break;
            default:
                fprintf(stderr, "Bad '%s' value\n", parameter);
                return STATUS_INVALID_VALUE;
        }

        if (t.get_token(expr::TF_GET) != expr::TT_EOF)
        {
            fprintf(stderr, "Bad '%s' value\n", parameter);
            return STATUS_INVALID_VALUE;
        }

        *dst = ivalue;

        return STATUS_OK;
    }

    status_t parse_cmdline_float(float *dst, const char *val, const char *parameter)
    {
        LSPString in;
        if (!in.set_native(val))
        {
            fprintf(stderr, "Out of memory\n");
            return STATUS_NO_MEM;
        }

        io::InStringSequence is(&in);
        expr::Tokenizer t(&is);
        float fvalue;

        switch (t.get_token(expr::TF_GET))
        {
            case expr::TT_IVALUE: fvalue = t.int_value(); break;
            case expr::TT_FVALUE: fvalue = t.float_value(); break;
            default:
                fprintf(stderr, "Bad '%s' value\n", parameter);
                return STATUS_INVALID_VALUE;
        }

        if (t.get_token(expr::TF_GET) != expr::TT_EOF)
        {
            fprintf(stderr, "Bad '%s' value\n", parameter);
            return STATUS_INVALID_VALUE;
        }

        *dst = fvalue;

        return STATUS_OK;
    }

    status_t parse_cmdline_bool(bool *dst, const char *val, const char *parameter)
    {
        LSPString in;
        if (!in.set_native(val))
        {
            fprintf(stderr, "Out of memory\n");
            return STATUS_NO_MEM;
        }

        io::InStringSequence is(&in);
        expr::Tokenizer t(&is);
        bool bvalue;

        switch (t.get_token(expr::TF_GET))
        {
            case expr::TT_IVALUE: bvalue = t.int_value(); break;
            case expr::TT_FVALUE: bvalue = t.float_value() >= 0.5f; break;
            case expr::TT_TRUE: bvalue = true; break;
            case expr::TT_FALSE: bvalue = false; break;
            default:
                fprintf(stderr, "Bad '%s' value\n", parameter);
                return STATUS_INVALID_VALUE;
        }

        if (t.get_token(expr::TF_GET) != expr::TT_EOF)
        {
            fprintf(stderr, "Bad '%s' value\n", parameter);
            return STATUS_INVALID_VALUE;
        }

        *dst = bvalue;

        return STATUS_OK;
    }

    status_t parse_cmdline_enum(ssize_t *dst, const char *parameter, const char *val, const cfg_flag_t *flags)
    {
        LSPString in;
        if (!in.set_native(val))
        {
            fprintf(stderr, "Out of memory\n");
            return STATUS_NO_MEM;
        }

        io::InStringSequence is(&in);
        expr::Tokenizer t(&is);
        const cfg_flag_t *flag = NULL;

        switch (t.get_token(expr::TF_GET | expr::TF_XKEYWORDS))
        {
            case expr::TT_BAREWORD:
                if ((flag = find_config_flag(t.text_value(), flags)) == NULL)
                {
                    fprintf(stderr, "Bad '%s' value\n", parameter);
                    return STATUS_BAD_FORMAT;
                }
                break;

            default:
                fprintf(stderr, "Bad '%s' value\n", parameter);
                return STATUS_BAD_FORMAT;
        }

        if (t.get_token(expr::TF_GET) != expr::TT_EOF)
        {
            fprintf(stderr, "Bad '%s' value\n", parameter);
            return STATUS_INVALID_VALUE;
        }

        *dst = flag->value;

        return STATUS_OK;
    }

    status_t parse_cmdline(config_t *cfg, int argc, const char **argv)
    {
        status_t res;
        const char *cmd = argv[0], *val;
        lltl::pphash<char, char> options;

        // Read options to hash
        for (int i=1; i < argc; )
        {
            const char *opt = argv[i++];

            // Aliases
            for (const option_t *p = spike_bender::options; p->s_short != NULL; ++p)
                if (!strcmp(opt, p->s_short))
                {
                    opt = p->s_long;
                    break;
                }

            // Check arguments
            const char *xopt = opt;
            if (!strcmp(opt, "--help"))
                return print_usage(cmd, false);
            else if ((opt[0] != '-') || (opt[1] != '-'))
            {
                fprintf(stderr, "Invalid argument: %s\n", opt);
                return STATUS_BAD_ARGUMENTS;
            }
            else
                xopt = opt;

            // Parse options
            bool found = false;
            for (const option_t *p = spike_bender::options; p->s_short != NULL; ++p)
                if (!strcmp(xopt, p->s_long))
                {
                    if ((!p->s_flag) && (i >= argc))
                    {
                        fprintf(stderr, "Not defined value for option: %s\n", opt);
                        return STATUS_BAD_ARGUMENTS;
                    }

                    // Add option to settings map
                    val = (p->s_flag) ? NULL : argv[i++];
                    if (options.exists(xopt))
                    {
                        fprintf(stderr, "Duplicate option: %s\n", opt);
                        return STATUS_BAD_ARGUMENTS;
                    }

                    // Try to create option
                    if (!options.create(xopt, const_cast<char *>(val)))
                    {
                        fprintf(stderr, "Not enough memory\n");
                        return STATUS_NO_MEM;
                    }

                    found       = true;
                    break;
                }

            if (!found)
            {
                fprintf(stderr, "Invalid option: %s\n", opt);
                return STATUS_BAD_ARGUMENTS;
            }
        }

        // Mandatory parameters
        if ((val = options.get("--in-file")) != NULL)
            cfg->sInFile.set_native(val);
        else
        {
            fprintf(stderr, "Input file name required\n");
            return STATUS_BAD_ARGUMENTS;
        }

        if ((val = options.get("--out-file")) != NULL)
            cfg->sOutFile.set_native(val);
        else
        {
            fprintf(stderr, "Output file name required\n");
            return STATUS_BAD_ARGUMENTS;
        }

        // Parse other parameters
        if ((val = options.get("--srate")) != NULL)
        {
            if ((res = parse_cmdline_int(&cfg->nSampleRate, val, "output sample rate")) != STATUS_OK)
                return res;
        }

        return STATUS_OK;
    }

} /* namespace spike_bender */






