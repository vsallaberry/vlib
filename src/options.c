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
 *
 * TODO
 *   * --help=unknown_filter should give an error
 *   * optimization of long option aliases management in opt_usage() ?
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#include "version.h"

#ifndef BUILD_CURSES
# define BUILD_CURSES 0
#endif
#if BUILD_CURSES
# include <dlfcn.h>
//# include <curses.h>
//# include <term.h>
//# include <termios.h>
#endif

#include "vlib/options.h"
#include "vlib/util.h"

/* ** OPTIONS *********************************************************************************/

#define OPT_USAGE_OPT_PAD 30

inline static int is_opt_end(const opt_options_desc_t * opt) {
    return opt == NULL
        || (opt->short_opt == 0 && opt->desc == NULL);
}

inline static int is_valid_short_opt(int c) {
    return c && isascii(c) && isgraph(c);
}

inline static int is_opt_section(int c) {
    return (c >= OPT_ID_SECTION && c <= OPT_ID_SECTION_MAX);
}

inline static int is_opt_arg(int c) {
    return (c >= OPT_ID_ARG && c <= OPT_ID_ARG_MAX);
}

inline static int is_opt_user(int c) {
    return (c >= OPT_ID_USER && c <= OPT_ID_USER_MAX);
}

inline static int is_valid_opt(int c) {
    return is_opt_user(c)
        || is_opt_arg(c)
        || is_opt_section(c)
        || is_valid_short_opt(c);
}

static int get_registered_opt(int c, const opt_config_t * opt_config) {
    for (int i_opt = 0; !is_opt_end(&opt_config->opt_desc[i_opt]); i_opt++) {
        if (opt_config->opt_desc[i_opt].short_opt == c) {
            return i_opt;
        }
    }
    return -1;
}

static int get_registered_short_opt(int c, const opt_config_t * opt_config) {
    if (!is_valid_short_opt(c)) {
        return -1;
    }
    return get_registered_opt(c, opt_config);
}

static int get_registered_long_opt(const char * long_opt, const char ** popt_arg,
                                   const opt_config_t * opt_config) {
    if (long_opt == NULL)
        return -1;
    for (int i_opt = 0; !is_opt_end(&opt_config->opt_desc[i_opt]); i_opt++) {
        const char * cur_longopt = opt_config->opt_desc[i_opt].long_opt;
        size_t len;

        if (cur_longopt == NULL)
            continue ;
        len = strlen(cur_longopt);
        if (!strncmp(long_opt, cur_longopt, len) && (long_opt[len] == 0 || long_opt[len] == '=')) {
            if (popt_arg) {
                *popt_arg = long_opt[len] == '=' ? long_opt + len + 1 : NULL;
            }
            return i_opt;
        }
    }
    return -1;
}

static int opt_alias(int i_opt, const opt_config_t * opt_config) {
    const opt_options_desc_t * opt = &opt_config->opt_desc[i_opt];
    /* look for same short_opt defined before index i_opt in desc array */
    if (opt->desc == NULL && opt->long_opt != NULL && opt->arg == NULL) {
        for (int i = 0; i < i_opt; i++) {
            if (opt_config->opt_desc[i].short_opt == opt->short_opt) {
                return i;
            }
        }
    }
    return -1;
}

static int opt_error(int exit_code, const opt_config_t * opt_config, int show_usage,
                     const char * fmt, ...) {
    if (opt_config && (opt_config->flags & OPT_FLAG_SILENT) == 0) {
        if (fmt != NULL) {
            va_list arg;
            va_start(arg, fmt);
            vfprintf(stderr, fmt, arg);
            va_end(arg);
        }
        if (show_usage) {
            return opt_usage(exit_code, opt_config, NULL);
        }
    }
    return exit_code;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static unsigned int get_max_columns(FILE * out, const opt_config_t * opt_config) {
    unsigned int max_columns = 80;
    (void) opt_config;
#if BUILD_CURSES
    if (isatty(fileno(out))) {
        int ret;
        void * lib = NULL;
        int (*setup)(char*, int, int*);
        int (*getnum)(char*);
        char * libs[] = { "libncurses.so", "libcurses.so", "libncurses.dylib", "libcurses.dylib",
                          "libtinfo.so", "libtinfo.dylib",
                          "libncurses.so.5", "libcurses.so.5", "libtinfo.so.5", NULL };
        for (char ** path = libs; *path && (lib = dlopen(*path, RTLD_LAZY)) == NULL; path++)
            ; /* loop */
        if (lib) {
            if ((setup = (int(*)(char*,int,int*)) dlsym(lib, "setupterm"))
            &&  (getnum = (int(*)(char*)) dlsym(lib, "tigetnum"))) {
                if (setup(NULL, fileno(out), &ret) < 0) {
                    /* maybe not needed to printf error, could use column=80 silently */
                    if      (ret == 1)  fprintf(out, "setupterm(): term is hardcopy.\n");
                    else if (ret == 0)  fprintf(out, "setupterm(): term not found.\n");
                    else if (ret == -1) fprintf(out, "setupterm(): term db not found.\n");
                    else                fprintf(out, "setupterm(): unknown error.\n");
                } else if (((ret = getnum("cols")) > 0
                            || (ret = getnum("columns")) > 0
                            || (ret = getnum("COLUMNS")) > 0)
                           && ret > OPT_USAGE_OPT_PAD + 10) {
                    max_columns = ret;
                }
            }
            dlclose(lib);
        }
    }
#endif
    return max_columns;
}
#pragma GCC diagnostic pop

static int opt_usage_filter(const char * filter, int i_opt, int i_section,
                            const opt_config_t * opt_config) {
    const opt_options_desc_t * opt = &opt_config->opt_desc[i_opt];
    const char * next = filter, * token, * longopt;
    const char * section = i_section >= 0 ? opt_config->opt_desc[i_section].arg : NULL;
    size_t len;

    if (filter == NULL)
        return 1;

    longopt = is_opt_section(opt->short_opt) ? NULL : opt->long_opt;

    while ((len = strtok_ro_r(&token, ",:|;&", &next, NULL, 0)) > 0 || *next) {
        if (!len)
            continue ;

        /* check long-option alias */
        if (longopt && (strncasecmp(longopt, token, len) || longopt[len] != 0)) {
            for (const opt_options_desc_t * opt2 = opt + 1; !is_opt_end(opt2); ++opt2) {
                if (opt2->short_opt == opt->short_opt && opt2->long_opt
                && !strncasecmp(opt2->long_opt, token, len) && opt2->long_opt[len] == 0) {
                    token = opt->long_opt;
                    len = strlen(token);
                }
            }
        }

        /* there is a match if short_opt, or long_opt, or 'all' or current section is given */
        if ((len == 1 && *token == opt->short_opt)
        ||  (len == 3 && !strncasecmp(token, "all", 3))
        ||  (longopt && !strncasecmp(token, longopt, len) && longopt[len] == 0)
        ||  (section && !strncasecmp(token, section, len) && section[len] == 0)) {
            return 1;
        }
    }

    return 0;
}

static void opt_print_usage_summary(const opt_config_t * opt_config,
                                    FILE * out, unsigned int max_columns) {
    const char *    start_name;
    unsigned int    n_printed, pad;
    int             i_opt, i_firstarg = -1;

    /* don't print anything if requested */
    if ((opt_config->flags & OPT_FLAG_NOUSAGE) != 0)
        return ;

    /* print program name, version and usage summary */
    if ((start_name = strrchr(*opt_config->argv, '/')) == NULL) {
        start_name = *opt_config->argv;
    } else {
        start_name++;
    }
    if (opt_config->version_string && *opt_config->version_string)
        fprintf(out, "%s\n\n", opt_config->version_string);
    n_printed = pad = fprintf(out, "Usage: %s ", start_name);

    /* print only simple usage summary if requested */
    if ((opt_config->flags & OPT_FLAG_SIMPLEUSAGE) != 0) {
        fprintf(out, "[options] [arguments]\n");
        return ;
    }

    /* first pass to print short options without arguments */
    i_opt = 0;
    for (const opt_options_desc_t * opt = opt_config->opt_desc; !is_opt_end(opt); ++opt, ++i_opt) {
        if (opt->arg == NULL && is_valid_short_opt(opt->short_opt)
        &&  opt_alias(i_opt, opt_config) < 0) {
            if (n_printed + 2 > max_columns) {
                fputc(']', out);
                for (n_printed = 0; n_printed < pad; n_printed++)
                    fputc(' ', out);
            }
            if (n_printed == pad)
                n_printed += fprintf(out, "[-");
            if (fputc(opt->short_opt, out) != EOF)
                n_printed++;
        }
    }
    if (n_printed > pad)
        n_printed += fprintf(out, "]");

    /* second pass to print simple arguments or short options with arguments */
    i_opt = 0;
    for (const opt_options_desc_t * opt = opt_config->opt_desc; !is_opt_end(opt); ++opt, ++i_opt) {
        int isarg = 0;
        if (opt->arg != NULL && (is_valid_short_opt(opt->short_opt)
                                 || (isarg = is_opt_arg(opt->short_opt)))
        &&  opt_alias(i_opt, opt_config) < 0) {
            /* compute length of next item */
            unsigned int len = strlen(opt->arg) + 1; /* will print at least " arg" */
            if (!isarg) {
               len += 4 + (*opt->arg != '[' ? 2 : 0); /* will print "[-Xarg]" or "[-X<arg>]" */
            } else if (i_firstarg < 0) {
                len += 5; /* will printf additional " [--]" string */
            }
            /* check columns limit */
            if (n_printed + len > max_columns) {
                fputc('\n', out);
                for (n_printed = 1; n_printed < pad; n_printed++)
                    fputc(' ', out);
            }
            /* display it */
            if (isarg) {
                if (i_firstarg < 0) {
                    n_printed += fprintf(out, " [--]");
                }
                i_firstarg = i_opt;
                n_printed += fprintf(out, " %s", opt->arg);
            } else if (*opt->arg != '[') {
                n_printed += fprintf(out, " [-%c<%s>]", opt->short_opt, opt->arg);
            } else {
                n_printed += fprintf(out, " [-%c%s]", opt->short_opt, opt->arg);
            }
        }
    }
    /* print " [--]" if not already done */
    if (i_firstarg < 0) {
        if (n_printed + 5 > max_columns) {
            fputc('\n', out);
            for (n_printed = 1; n_printed < pad; n_printed++)
                fputc(' ', out);
        }
        n_printed += fprintf(out, " [--]");
    }
    fprintf(out, "\n");
}

int opt_usage(int exit_status, const opt_config_t * opt_config, const char * filter) {
    char            desc_buffer[4096];
    FILE *          out = stdout;
    unsigned int    max_columns;
    int             i_opt, i_current_section = -1;

    /* sanity checks */
    fflush(NULL);
    if (opt_config == NULL || opt_config->argv == NULL || opt_config->opt_desc == NULL) {
        return opt_error(OPT_ERROR(OPT_EFAULT), opt_config, 0,
                         "%s/%s(): opt_config or opt_desc or argv is NULL!\n", __FILE__, __func__);
    }

    /* exit if OPT_FLAG_SILENT */
    if ((opt_config->flags & OPT_FLAG_SILENT) != 0) {
        return exit_status;
    }

    /* if this is an error: use stderr and put a blank between error message and usage */
    if (OPT_IS_ERROR(exit_status) != 0) {
        out = stderr;
        fprintf(out, "\n");
    }

    /* get max columns usable for display */
    max_columns = get_max_columns(out, opt_config);

    /* print program name, version and usage summary */
    opt_print_usage_summary(opt_config, out, max_columns);

    /* print list of options with their descrption */
    i_opt = 0;
    for (const opt_options_desc_t * opt = opt_config->opt_desc; !is_opt_end(opt); ++opt, ++i_opt) {
        int             n_printed = 0;
        const char *    token;
        const char *    next;
        size_t          len;
        int             eol_shift = 0;
        size_t          desc_size = 0, * psize = NULL;
        int             is_section;

        /* keep current section index, and stop if we should only print main section */
        if ((is_section = is_opt_section(opt->short_opt))) {
            if (filter == NULL && (i_current_section >= 0 || opt != opt_config->opt_desc)
            &&  (opt_config->flags & OPT_FLAG_MAINSECTION) != 0)
                break ;
            i_current_section = i_opt;
        }
        /* handle the usage filter, and ignore current option if no match */
        if (!opt_usage_filter(filter, i_opt, i_current_section, opt_config)) {
            continue ;
        }
        if (!is_section) {
            /* skip option if this is an alias */
            if (opt_alias(i_opt, opt_config) >= 0)
                continue;
            /* short options */
            n_printed += fprintf(out, "  ");
            if (is_valid_short_opt(opt->short_opt)) {
                n_printed += fprintf(out, "-%c", opt->short_opt);
            }
            /* long option */
            if (opt->long_opt != NULL) {
                n_printed += fprintf(out, "%s--%s", n_printed > 2 ? ", " : "", opt->long_opt);
            }
            /* look for long option aliases */
            for (const opt_options_desc_t *opt2 = opt + 1; !is_opt_end(opt2); ++opt2) {
                if (opt2->short_opt == opt->short_opt && opt2->long_opt != NULL
                && opt2->desc == NULL) {
                    n_printed += fprintf(out, "%s--%s", n_printed > 2 ? ", " : "", opt2->long_opt);
                }
            }
            /* option argument name */
            if (opt->arg) {
                if (n_printed > 2 && fputc(' ', out) != EOF)
                    n_printed++;
                n_printed += fprintf(out, "%s", opt->arg);
            }
        }
        /* getting dynamic option description */
        if (opt_config->callback && is_valid_opt(opt->short_opt)) {
            int i_desc_size = sizeof(desc_buffer);
            if (!OPT_IS_CONTINUE(
                  opt_config->callback(OPT_DESCRIBE_OPTION | opt->short_opt,
                                       (const char *) desc_buffer, &i_desc_size, opt_config))) {
                i_desc_size = 0;
            }
            desc_size = i_desc_size;
        }
        /* skip option description and process next option if no description */
        next = opt->desc;
        if ((!next || !*next) && desc_size <= 0) {
            fputc('\n', out);
            continue ;
        }
        /* print EOL if characters printed exceed padding */
        if (n_printed > OPT_USAGE_OPT_PAD) {
            fputc('\n', out);
            n_printed = 0;
        }
        /* parsing option descriptions, splitting them into words and fix alignment */
        while (1) {
            if ((len = strtok_ro_r(&token, " \n-,;:/?=+*\\", &next, psize,
                                   VLIB_STRTOK_INCLUDE_SEP)) <= 0) {
                if (desc_size > 0 && psize == NULL) {
                    /* switch to dynamic buffer if static buffer is finished */
                    next = desc_buffer;
                    psize = &desc_size;
                    continue ;
                }
                break ;
            }
            /* insert EOL if it does not fit in max_columns */
            if (*token != '\n' && len + n_printed > max_columns) {
                fputc('\n', out);
                n_printed = 0;
                eol_shift = 2;
            }
            /* Align description if needed */
            if (!is_section) {
                while (n_printed < OPT_USAGE_OPT_PAD + eol_shift) {
                    fputc(' ', out);
                    n_printed++;
                }
                if (!eol_shift && fputs(": ", out) != EOF)
                    n_printed += 2;
            }
            eol_shift = 2;
            /* print description */
            n_printed += fwrite(token, 1, len, out);
            if (token[len-1] == '\n')
                n_printed = 0;
        }
        /* EOL before processing next option */
        fputc('\n', out);
    }
    fputc('\n', out);
    return exit_status;
}

int opt_parse_options(const opt_config_t * opt_config) {
    const char *const*          argv;
    const opt_options_desc_t *  desc;

    /* sanity checks */
    if (opt_config == NULL || (desc = opt_config->opt_desc) == NULL
    ||  (argv = opt_config->argv) == NULL) {
        return opt_error(OPT_ERROR(OPT_EFAULT), opt_config, 0,
                         "%s/%s(): opt_config or opt_desc or argv is NULL!\n", __FILE__, __func__);
    }

    /* Analysing each argument of commandline. */
    for(int i_argv = 1, stop_options = 0; i_argv < opt_config->argc; i_argv++) {
        int result;
        /* Check Options (starting with '-') */
        if (!stop_options && *argv[i_argv] == '-' && argv[i_argv][1]) {
            const char      short_fake[2]   = { 'a', 0 };
            const char *    short_opts      = argv[i_argv] + 1;
            const char *    opt_arg         = NULL;
            const char *    long_opt        = NULL;
            int             i_opt;

            /* Check for a second '-' : long option. */
	        if (*short_opts == '-') {
                /* The '--' special option will stop taking words starting with '-' as options */
		        if (!short_opts[1]) {
		            stop_options = 1;
		            continue ;
		        }
                /* Check if long option is registered (get index) */
                if ((i_opt = get_registered_long_opt(argv[i_argv] + 2, &opt_arg, opt_config)) < 0) {
                    return opt_error(OPT_ERROR(OPT_ELONG), opt_config, 1,
                                     "error: unknown option '%s'\n", argv[i_argv]);
                }
                long_opt = desc[i_opt].long_opt;
                /* Check alias */
                if ((result = opt_alias(i_opt, opt_config)) >= 0)
                    i_opt = result;
                /* Long option is found, the matching short_opt is in opt_val. */
                if (desc[i_opt].short_opt == 0) {
                    return opt_error(OPT_ERROR(OPT_ELONGID), opt_config, 1,
                                     "error: bad 'short_opt' value for option '%s'\n", long_opt);
                }
                short_opts = short_fake;
            }

            /* Proceed each short option, or the long option once. */
            for (const char * popt = short_opts; *popt; popt++, opt_arg = NULL) {
                /* Check if short option is reconized */
                if (long_opt == NULL && (i_opt = get_registered_short_opt(*popt, opt_config)) < 0) {
                    return opt_error(OPT_ERROR(OPT_ESHORT), opt_config, 1,
                                     "error: unknown option '-%c'\n", *popt);
                }
                /* Detect option argument */
                if (opt_arg == NULL) {
                    if (/* not last_char_of_current_param OR not the last param. of argv */
                        (popt[1] || (i_argv + 1 < opt_config->argc))
                        /* AND if option parameter is declared in opt_options_desc_t */
                        && desc[i_opt].arg != NULL
                        /* AND (option is mandatory or (last_opt_of_current and next_not_opt) */
                        && (*desc[i_opt].arg != '[' || popt[1] || *argv[i_argv+1] != '-')
                    ) {
                        if (popt[1]) {
                            opt_arg = ++popt;
                            while (*(popt+1))
                                popt++;
                        } else {
                            opt_arg = argv[++i_argv];
                        }
                        /* allow optional argument starting with '-' if prefixed with '\' */
                        if (desc[i_opt].arg != NULL && *opt_arg == '\\' && (opt_arg[1] == '-'))
                            opt_arg++;
                    }
                }
                /* Check presence of mandatory option argument */
                if (desc[i_opt].arg != NULL && *desc[i_opt].arg != '[' && opt_arg == NULL) {
                    return opt_error(OPT_ERROR(OPT_EOPTNOARG), opt_config, 1,
                                     "error: missing argument '%s' for option '-%c%s'\n",
                                     desc[i_opt].arg, long_opt != NULL ? '-' : desc[i_opt].short_opt,
                                     long_opt != NULL ? long_opt : "");
                }
                /* Check presence of unexpected option argument */
                if (opt_arg != NULL && desc[i_opt].arg == NULL) {
                    return opt_error(OPT_ERROR(OPT_EOPTARG), opt_config, 1,
                                     "error: unexpected argument '%s' for option '-%c%s'\n",
                                     opt_arg, long_opt != NULL ? '-' : desc[i_opt].short_opt,
                                     long_opt != NULL ? long_opt : "");
                }
                /* Call the callback if any */
                if (opt_config->callback) {
                    result = opt_config->callback(desc[i_opt].short_opt,
                                                  opt_arg, &i_argv, opt_config);
                    if (OPT_IS_ERROR(result)) {
                        return opt_error(OPT_ERROR(OPT_EBADOPT), opt_config, 1,
                                         "error: incorrect option '-%c%s'\n",
                                         long_opt != NULL ? '-' : desc[i_opt].short_opt,
                                         long_opt != NULL ? long_opt : "");
                    }
                    if (OPT_IS_EXIT_OK(result)) {
                        return OPT_EXIT_OK(result);
                    }
                }
            }
        } else {
            /* the argument is not an option. */
            if (opt_config->callback) {
                const char * arg = argv[i_argv];
                result = opt_config->callback(OPT_ID_ARG, arg, &i_argv, opt_config);
                if (OPT_IS_ERROR(result)) {
                    return opt_error(OPT_ERROR(OPT_EBADARG), opt_config, 1,
                                     "error: incorrect argument %s\n", arg);
                }
	            if (OPT_IS_EXIT_OK(result)) {
		            return OPT_EXIT_OK(result);
	            }
            }
        }
    }
    return OPT_CONTINUE(1);
}

int opt_describe_filter(int short_opt, const char * arg, int * i_argv,
                        const opt_config_t * opt_config) {
    int n = 0, ret;
    (void) short_opt;

    n += (ret = snprintf((char *)arg, *i_argv - n, "filter:'all")) > 0 ? ret : 0;
    for (const opt_options_desc_t * opt = opt_config->opt_desc; opt->short_opt || opt->desc; ++opt) {
        if (is_opt_section(opt->short_opt)) {
            n += (ret = snprintf((char *)arg + n, *i_argv - n, ",%s", opt->arg)) > 0 ? ret : 0;
        }
    }
    n += (ret = snprintf((char *)arg + n, *i_argv - n, ",<shortopt>,<longopt>'")) > 0 ? ret : 0;
    *i_argv = n;
    return OPT_CONTINUE(1);
}

const char * vlib_get_version() {
    return OPT_VERSION_STRING(BUILD_APPNAME, APP_VERSION, "git:" BUILD_GITREV);
}

#ifndef APP_INCLUDE_SOURCE
const char *const* vlib_get_source() {
    static const char * const source[] = {
        BUILD_APPNAME " source not included in this build.\n", NULL
    };
    return source;
}
#endif

