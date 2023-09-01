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

#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/expr/Tokenizer.h>
#include <lsp-plug.in/io/InStringSequence.h>
#include <lsp-plug.in/lltl/pphash.h>
#include <lsp-plug.in/stdlib/string.h>
#include <lsp-plug.in/stdlib/stdio.h>

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
        { "-dr",  "--dynamic-range",        false,     "Dynamic range of the compressor (in dB, 6 dB by default)"                               },
        { "-ep",  "--eliminate-peaks",      false,     "The threshold above which all peaks are eliminated (in dB, 1 dB by default, off if not positive)"     },
        { "-if",  "--in-file",              false,     "The path to the input file"                                                             },
        { "-k",   "--knee",                 false,     "Knee of the compressor (in dB, 3 dB by default)"                                        },
        { "-n",   "--normalize",            false,     "Set normalization mode (none, above, below, always, none by default)"                   },
        { "-ng",  "--norm-gain",            false,     "Set normalization peak gain (in dB, 0 dB by default)"                                   },
        { "-np",  "--num-passes",           false,     "Number of passes, 1 by default"                                                         },
        { "-of",  "--out-file",             false,     "The path to the output file"                                                            },
        { "-r",   "--reactivity",           false,     "Reactivity of the compressor (in ms, 40 ms by default)"                                 },
        { "-sr",  "--srate",                false,     "Sample rate of output (processed) file, optional"                                       },
        { "-wf",  "--weighting",            false,     "Frequency weighting function (none, a, b, c, d, k, none by default)"                    },

        { NULL, NULL, false, NULL }
    };

    const cfg_flag_t weighting_flags[] =
    {
        { "none",   NO_WEIGHT   },
        { "a",      A_WEIGHT    },
        { "b",      B_WEIGHT    },
        { "c",      A_WEIGHT    },
        { "d",      B_WEIGHT    },
        { "k",      K_WEIGHT    },
        { NULL,     0           }
    };

    const cfg_flag_t normalize_flags[] =
    {
        { "none",   NORM_NONE   },
        { "above",  NORM_ABOVE  },
        { "below",  NORM_BELOW  },
        { "always", NORM_ALWAYS },
        { NULL,     0           }
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
                fprintf(stderr, "Bad '%s' value: '%s'\n", parameter, val);
                return STATUS_INVALID_VALUE;
        }

        if (t.get_token(expr::TF_GET) != expr::TT_EOF)
        {
            fprintf(stderr, "Bad '%s' value '%s'\n", parameter, val);
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
                fprintf(stderr, "Bad '%s' value: '%s'\n", parameter, val);
                return STATUS_INVALID_VALUE;
        }

        if (t.get_token(expr::TF_GET) != expr::TT_EOF)
        {
            fprintf(stderr, "Bad '%s' value '%s'\n", parameter, val);
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
                fprintf(stderr, "Bad '%s' value '%s'\n", parameter, val);
                return STATUS_INVALID_VALUE;
        }

        if (t.get_token(expr::TF_GET) != expr::TT_EOF)
        {
            fprintf(stderr, "Bad '%s' value '%s'\n", parameter, val);
            return STATUS_INVALID_VALUE;
        }

        *dst = bvalue;

        return STATUS_OK;
    }

    template <class T>
    status_t parse_cmdline_enum(T *dst, const char *val, const char *parameter, const cfg_flag_t *flags)
    {
        LSPString in;
        if (!in.set_native(val))
        {
            fprintf(stderr, "Out of memory\n");
            return STATUS_NO_MEM;
        }
        in.tolower();

        for (const cfg_flag_t *flag = flags; flag->name != NULL; ++flag)
        {
            if (in.equals_ascii(flag->name))
            {
                *dst = T(flag->value);
                return STATUS_OK;
            }
        }

        fprintf(stderr, "Bad '%s' value: '%s'\n", parameter, val);

        return STATUS_INVALID_VALUE;
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
        if ((val = options.get("--dynamic-range")) != NULL)
        {
            if ((res = parse_cmdline_float(&cfg->fRange, val, "dynamic range")) != STATUS_OK)
                return res;
            if (cfg->fRange <= 0.0f)
            {
                fprintf(stderr, "Bad dynamic range value, should be positive\n");
                return STATUS_BAD_ARGUMENTS;
            }
        }
        if ((val = options.get("--knee")) != NULL)
        {
            if ((res = parse_cmdline_float(&cfg->fKnee, val, "knee")) != STATUS_OK)
                return res;
            if (cfg->fRange < 0.0f)
            {
                fprintf(stderr, "Bad knee value, should be non-negative\n");
                return STATUS_BAD_ARGUMENTS;
            }
        }
        if ((val = options.get("--num-passes")) != NULL)
        {
            if ((res = parse_cmdline_int(&cfg->nPasses, val, "number of passes")) != STATUS_OK)
                return res;
            if (cfg->nPasses <= 0)
            {
                fprintf(stderr, "Invalid number of passes, should be positive\n");
                return STATUS_BAD_ARGUMENTS;
            }
        }
        if ((val = options.get("--reactivity")) != NULL)
        {
            if ((res = parse_cmdline_float(&cfg->fReactivity, val, "reactivity")) != STATUS_OK)
                return res;
            if (cfg->fReactivity < 0.0f)
            {
                fprintf(stderr, "Bad reactivity value, should be non-negative\n");
                return STATUS_BAD_ARGUMENTS;
            }
        }
        if ((val = options.get("--weighting")) != NULL)
        {
            if ((res = parse_cmdline_enum(&cfg->enWeighting, val, "weighting function", weighting_flags)) != STATUS_OK)
                return res;
        }
        if ((val = options.get("--normalize")) != NULL)
        {
            if ((res = parse_cmdline_enum(&cfg->enNormalize, val, "normalize", normalize_flags)) != STATUS_OK)
                return res;
        }
        if ((val = options.get("--norm-gain")) != NULL)
        {
            if ((res = parse_cmdline_float(&cfg->fNormGain, val, "norm-gain")) != STATUS_OK)
                return res;
        }
        if ((val = options.get("--eliminate-peaks")) != NULL)
        {
            if ((res = parse_cmdline_float(&cfg->fPeakThresh, val, "eliminate-peaks")) != STATUS_OK)
                return res;
            cfg->fPeakThresh  = dspu::db_to_gain(cfg->fPeakThresh);
        }

        return STATUS_OK;
    }

} /* namespace spike_bender */






