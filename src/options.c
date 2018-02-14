/*
 * Copyright (C) 2017-2018 Vincent Sallaberry
 * vlib <https://github.com/vsallaberry/vlib>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/* ------------------------------------------------------------------------
 * Simple command line options management.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "vlib/options.h"
#include "vlib/util.h"

#include "version.h"

/* ** OPTIONS *********************************************************************************/

#define OPT_USAGE_OPT_PAD 30

static int is_valid_short_opt(int c) {
    return c && isascii(c) && isgraph(c);
}

static int get_registered_short_opt(int c, const opt_config_t * opt_config) {
    if (!is_valid_short_opt(c)) {
        return -1;
    }
    for (int i_opt = 0; opt_config->opt_desc[i_opt].short_opt; i_opt++) {
        if (opt_config->opt_desc[i_opt].short_opt == c) {
            return i_opt;
        }
    }
    return -1;
}

static int get_registered_long_opt(const char * long_opt, const opt_config_t * opt_config) {
    for (int i_opt = 0; opt_config->opt_desc[i_opt].short_opt; i_opt++) {
        const char * cur_longopt = opt_config->opt_desc[i_opt].long_opt;
        size_t len;

        if (cur_longopt == NULL)
            continue ;
        len = strlen(cur_longopt);
        if (!strncmp(long_opt, cur_longopt, len) && (long_opt[len] == 0)) {
            return i_opt;
        }
    }
    return -1;
}

int opt_usage(int exit_status, const opt_config_t * opt_config) {
    FILE *                      out         = exit_status ? stderr : stdout;
    const char *                start_name  = strrchr(*opt_config->argv, '/');
    const opt_options_desc_t *  opt_desc    = opt_config->opt_desc;

    if (start_name == NULL) {
    	start_name = *opt_config->argv;
    } else {
	    start_name++;
    }

    if (exit_status != 0) {
        fprintf(out, "\n");
    }
    fprintf(out, "%s  - %s\n\n", start_name, opt_config->version_string);
    fprintf(out, "Usage: %s [<options>] [<arguments>]\n", start_name);
    for (int i_opt = 0; opt_desc[i_opt].short_opt; i_opt++) {
    	int n_printed = 0;
        const char * token;
        const char * next;
        int len;
        int eol_shift = 0;

        n_printed += fprintf(out, "  ");
        if (is_valid_short_opt(opt_desc[i_opt].short_opt)) {
    	    n_printed += fprintf(out, "-%c%s", opt_desc[i_opt].short_opt,
                                      opt_desc[i_opt].long_opt != NULL ? ", " : "");
        }
        if (opt_desc[i_opt].long_opt != NULL) {
	        n_printed += fprintf(out, "--%s", opt_desc[i_opt].long_opt);
        }
	    if (opt_desc[i_opt].arg) {
	        n_printed += fprintf(out, " %s", opt_desc[i_opt].arg);
	    }

        if (n_printed > OPT_USAGE_OPT_PAD) {
            fputc('\n', out);
            n_printed = 0;
        }
        next = opt_desc[i_opt].desc;
        if (!next || !*next) {
            fputc('\n', out);
            continue ;
        }
        while ((len = strtok_ro_r(&token, "\n", &next, NULL, 0)) > 0) {
            while (n_printed++ < OPT_USAGE_OPT_PAD) {
	            fputc(' ', out);
	        }
            if (!eol_shift)
                fputs(": ", out);
            while (len--)
                fputc(*token++, out);
            fputc('\n', out);
            n_printed = eol_shift = -2;
        }
    }
    fprintf(out, "\n");
    return exit_status;
}

int opt_parse_options(const opt_config_t * opt_config) {
    if (opt_config == NULL || opt_config->opt_desc == NULL || opt_config->argv == NULL) {
        fprintf(stderr, "%s/%s(): opt_config or opt_desc or argv is NULL!\n", __FILE__, __func__);
        return 10;
    }
    const char *const*          argv    = opt_config->argv;
    const opt_options_desc_t *  desc    = opt_config->opt_desc;
	int                         result;

    /* Analysing each argument of commandline. */
    for(int i_argv = 1, stop_options = 0; i_argv < opt_config->argc; i_argv++) {
        /* Check Options (starting with '-') */
        if (!stop_options && *argv[i_argv] == '-' && argv[i_argv][1]) {
            const char      short_fake[2]   = { 'a', 0 };
            const char *    short_args      = argv[i_argv] + 1;
            int             long_opt_val    = 0;
            int             i_opt;

            /* Check for a second '-' : long option. */
	        if (*short_args == '-') {
                /* The '--' special option will stop taking words starting with '-' as options */
		        if (!short_args[1]) {
		            stop_options = 1;
		            continue ;
		        }
                /* Check if long option is registered (get index) */
                if ((i_opt = get_registered_long_opt(argv[i_argv] + 2, opt_config)) < 0) {
                    fprintf(stderr, "error: unknown option '%s'\n", argv[i_argv]);
                    return opt_usage(-5, opt_config);
                }
                /* Long option is found, the matching short_opt is in opt_val. */
	            long_opt_val = desc[i_opt].short_opt;
                short_args = short_fake;
            }

            /* Proceed each short option. */
            for (const char * arg = short_args; *arg; arg++) {
                /* Check if short option is reconized */
                if (!long_opt_val && get_registered_short_opt(*arg, opt_config) < 0) {
                    fprintf(stderr, "error: unknown option '-%c'\n", *arg);
                    return opt_usage(-2, opt_config);
                }

                /* Pass the argument if any and if no option follows in the same word.
                 * The handler will have responsibility to shift i_argv accordinally. */
                if (opt_config->callback) {
                    result = opt_config->callback(long_opt_val ? long_opt_val : *arg,
                                                  i_argv + 1 < opt_config->argc && !arg[1]
                                                    ? argv[i_argv + 1] : NULL,
                                                  &i_argv, opt_config);
                    if (result < 0) {
                        if (long_opt_val)
                            fprintf(stderr, "error: incorrect option '-%s'\n", arg);
                        else
                            fprintf(stderr, "error: incorrect option '-%c'\n", *arg);
                        return opt_usage(-3, opt_config);
                    }
                    if (result == 0) {
                        return 0;
                    }
                }
            }
        } else {
            /* the argument is not an option. */
            if (opt_config->callback) {
                const char * arg = argv[i_argv];
                result = opt_config->callback(0, arg, &i_argv, opt_config);
                if (result < 0) {
                    fprintf(stderr, "error: incorrect argument %s\n", arg);
                    return opt_usage(-4, opt_config);
                }
	            if (result == 0) {
		            return 0;
	            }
            }
        }
    }
    return 1;
}

const char * vlib_get_version() {
    return BUILD_APPNAME " v" APP_VERSION " built on " __DATE__ ", " __TIME__ " \
           from git-rev " BUILD_GITREV;
}

#ifndef APP_INCLUDE_SOURCE
const char *const* vlib_get_source() {
    static const char * const source[] = { "vlib source not included in this build.\n", NULL };
    return source;
}
#endif

