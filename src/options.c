/*
 * Copyright (C) 2017-2020 Vincent Sallaberry
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
 *   * cleaning
 *   * optimization of long option aliases management in opt_usage() ?
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <fnmatch.h>

#include "vlib/options.h"
#include "vlib/util.h"
#include "vlib/log.h"
#include "vlib/thread.h"
#include "vlib/term.h"

#include "version.h"
#include "vlib_private.h"

/* ** OPTIONS *********************************************************************************/
#define OPT_USAGE_SUMUP_END_DESC1   " [--"
#define OPT_USAGE_SUMUP_END_DESC2       "<long-option>"
#define OPT_USAGE_SUMUP_END_DESC3                    "[=value]] [--]"
#define OPT_USAGE_SUMUP_END_DESC \
            OPT_USAGE_SUMUP_END_DESC1 OPT_USAGE_SUMUP_END_DESC2 OPT_USAGE_SUMUP_END_DESC3

#define OPT_USAGE_LOGLEVEL          LOG_LVL_INFO

/* default values for options colors, can be overriden in opt_config_t */
#define OPT_COLOR_SHORTOPT          VCOLOR_BUILD(VCOLOR_GREEN, VCOLOR_EMPTY, VCOLOR_BOLD)
#define OPT_COLOR_USE_SHORTOPT      OPT_COLOR_SHORTOPT
#define OPT_COLOR_LONGOPT           VCOLOR_BUILD(VCOLOR_CYAN, VCOLOR_EMPTY, VCOLOR_EMPTY)
#define OPT_COLOR_USE_LONGOPT       OPT_COLOR_LONGOPT
#define OPT_COLOR_ARG               VCOLOR_BUILD(VCOLOR_YELLOW, VCOLOR_EMPTY, VCOLOR_EMPTY)
#define OPT_COLOR_USE_ARG           VCOLOR_BUILD(VCOLOR_EMPTY, VCOLOR_EMPTY, VCOLOR_EMPTY)
#define OPT_COLOR_ERROR             VCOLOR_BUILD(VCOLOR_RED, VCOLOR_EMPTY, VCOLOR_BOLD)
#define OPT_COLOR_SECTION           VCOLOR_BUILD(VCOLOR_EMPTY, VCOLOR_EMPTY, VCOLOR_BOLD)
#define OPT_COLOR_DESC_TRUNC        OPT_COLOR_SECTION

#define OPT_COLOR_3ARGS(fd, colors) vterm_color(fd, VCOLOR_GET_FORE(colors)), \
                                    vterm_color(fd, VCOLOR_GET_BACK(colors)), \
                                    vterm_color(fd, VCOLOR_GET_STYLE(colors))

inline static int is_opt_end(const opt_options_desc_t * opt) {
    return opt == NULL || opt->short_opt == OPT_ID_END;
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

static int opt_check_opt_config(opt_config_t * opt_config) {
    if (opt_config == NULL)
        return -1;
    /* detect if macro was used to initialize opt_config, use defaults if not */
    if ((opt_config->flags & OPT_FLAG_MACROINIT) == 0
    || opt_config->opt_structsz != (sizeof(opt_config_t) << 16 | sizeof(opt_options_desc_t))) {
        LOG_WARN(g_vlib_log, "OPT_INITIALIZER() not used, or version mismatch, "
                             "disabling dynamic alignement, log, colors");
        opt_config->desc_align = OPT_USAGE_DESC_ALIGNMENT;
        opt_config->desc_minlen = OPT_USAGE_DESC_MINLEN;
        opt_config->desc_head = OPT_USAGE_DESC_HEAD;
        opt_config->opt_head = OPT_USAGE_OPT_HEAD;
        opt_config->flags &= ~OPT_FLAG_COLOR;
        opt_config->opt_help_name = "help";
        opt_config->log = NULL;
        return -1;
    } else {
        const struct { vterm_colorset_t * ptr; vterm_colorset_t val; } colors[] = {
            { &opt_config->color_short, OPT_COLOR_SHORTOPT },
            { &opt_config->color_useshort, OPT_COLOR_USE_SHORTOPT },
            { &opt_config->color_long, OPT_COLOR_LONGOPT },
            { &opt_config->color_uselong, OPT_COLOR_USE_LONGOPT },
            { &opt_config->color_arg, OPT_COLOR_ARG },
            { &opt_config->color_usearg, OPT_COLOR_USE_ARG },
            { &opt_config->color_sect, OPT_COLOR_SECTION },
            { &opt_config->color_trunc, OPT_COLOR_DESC_TRUNC },
            { &opt_config->color_err, OPT_COLOR_ERROR }
        };
        unsigned int i;
        for (i = 0; i < sizeof(colors) / sizeof(*colors); ++i) {
            if (*(colors[i].ptr) == UINT_MAX)
                *(colors[i].ptr) = colors[i].val;
        }
        return 0;
    }
}

#define OPTERR_NONE         (0)
#define OPTERR_SHOW_USAGE   (1 << 0) /* show opt_usage() after error display */
#define OPTERR_PRINT_ERR    (1 << 1) /* prefix error message with "error: " */

static int opt_error(int exit_code, opt_config_t * opt_config, int flags,
                     const char * filter, const char * fmt, ...)
                    __attribute__((format(printf, 5, 6)));

static int opt_error(int exit_code, opt_config_t * opt_config, int flags,
                     const char * filter, const char * fmt, ...) {
    if (opt_config && (opt_config->flags & OPT_FLAG_SILENT) == 0) {
        FILE *  out = stderr;
        int     ret;
        if (opt_config->log != NULL) {
            if ( ! LOG_CAN_LOG(opt_config->log, LOG_LVL_ERROR)) {
                return exit_code;
            }
            if (opt_config->log->out != NULL) {
                out = opt_config->log->out;
            }
            flockfile(out);
            log_header(LOG_LVL_ERROR, opt_config->log, NULL, NULL, 0);
        } else {
            flockfile(out);
        }
        if (fmt != NULL) {
            va_list arg;
            if ((flags & OPTERR_PRINT_ERR) != 0) {
                vterm_putcolor((opt_config->flags & OPT_FLAG_COLOR) == 0
                               ? NULL : out, opt_config->color_err);
                fprintf(out, "error%s: ", vterm_color(fileno(out), VCOLOR_RESET));
            }
            va_start(arg, fmt);
            vfprintf(out, fmt, arg);
            va_end(arg);
        }
        funlockfile(out);
        if ((flags & OPTERR_SHOW_USAGE) != 0) {
            ret = opt_usage(exit_code, opt_config, filter);
            return ret;
        }
    }
    return exit_code;
}

static int opt_usage_filter(const char * filter, int i_opt, int i_section,
                            opt_config_t * opt_config) {
    char short_str[2] = { 0, 0 };
    const opt_options_desc_t * opt = &opt_config->opt_desc[i_opt];
    const char * next = filter, * token, * longopt;
    const char * section = i_section >= 0 ? opt_config->opt_desc[i_section].arg : NULL;
    size_t len;
    char token0[PATH_MAX];

    if (filter == NULL)
        return 1;

    longopt = is_opt_section(opt->short_opt) ? NULL : opt->long_opt;
    *short_str = opt->short_opt;

    while ((len = strtok_ro_r(&token, ",|;&", &next, NULL, 0)) > 0 || *next) {
        if (!len)
            continue ;

        /* copy token into 0-terminated char[] */
        strn0cpy(token0, token, len, sizeof(token0) / sizeof (char));

        /* check option description (prefixed by ':') */
        if (len > 1 && *token == ':') {
            char    desc[PATH_MAX];
            int     i_descsz        = sizeof(desc);
            size_t  end             = strn0cpy(desc, opt->desc, i_descsz, i_descsz);

            i_descsz -= end;
            if (opt_config->callback == NULL || !OPT_IS_CONTINUE(
                  opt_config->callback(OPT_DESCRIBE_OPTION | opt->short_opt,
                                       desc + end, &i_descsz, opt_config))) {
                desc[end] = 0;
            }
            if (fnmatch(token0 + 1, desc, FNM_CASEFOLD) == 0) {
                return 1;
            }
            continue ;
        }

        /* check long-option alias */
        if (longopt && fnmatch(token0, longopt, FNM_CASEFOLD)) {
            for (const opt_options_desc_t * opt2 = opt + 1; !is_opt_end(opt2); ++opt2) {
                if (opt2->short_opt == opt->short_opt && opt2->long_opt
                && !fnmatch(token0, opt2->long_opt, FNM_CASEFOLD)) {
                    token = opt->long_opt;
                    len = strlen(token);
                    strn0cpy(token0, token, len, sizeof(token0) / sizeof (char));
                }
            }
        }

        /* there is a match if short_opt, or long_opt, or 'all' or current section is given */
        if (len == 3 && !strncasecmp(token, "all", 3)) {
            return INT_MAX;
        }
        if ((is_valid_short_opt(opt->short_opt) && !fnmatch(token0, short_str, 0))
        ||  (longopt && !fnmatch(token0, longopt, FNM_CASEFOLD))
        ||  (section && !fnmatch(token0, section, FNM_CASEFOLD))) {
            return 1;
        }
    }

    return 0;
}

static size_t opt_newline(FILE * out, const opt_config_t * opt_config, int print_header) {
    if (opt_config->log != NULL && ! LOG_CAN_LOG(opt_config->log, OPT_USAGE_LOGLEVEL))
        return 0;
    fputc('\n', out);
	fflush(out);
    if (print_header && opt_config->log != NULL)
        log_header(OPT_USAGE_LOGLEVEL, opt_config->log, NULL, NULL, 0);
    return 0;
}

static void opt_print_usage_summary(
                        const opt_config_t *        opt_config,
                        FILE *                      out,
                        unsigned int                max_columns,
                        unsigned int                max_optlen) {
    const char *    start_name;
    unsigned int    n_printed, pad;
    int             i_opt, i_firstarg = -1;
    int             fd = (opt_config->flags & OPT_FLAG_COLOR) != 0 ? fileno(out) : -1;

    /* don't print anything if requested */
    if ((opt_config->flags & OPT_FLAG_NOUSAGE) != 0
    ||  (opt_config->log != NULL && ! LOG_CAN_LOG(opt_config->log, OPT_USAGE_LOGLEVEL)))
        return ;

    /* print program name, version and usage summary */
    if ((start_name = strrchr(*opt_config->argv, '/')) == NULL) {
        start_name = *opt_config->argv;
    } else {
        start_name++;
    }
    if (opt_config->version_string && *opt_config->version_string) {
        const char * next = opt_config->version_string, * token;
        size_t len;
        while ((len = strtok_ro_r(&token, "\n", &next, NULL, 0)) > 0 || *next) {
            fwrite(token, 1, len, out);
            opt_newline(out, opt_config, 1);
        }
        opt_newline(out, opt_config, 1);
    }

    vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_sect);
    n_printed = fprintf(out, "Usage:");
    fputs(vterm_color(fd, VCOLOR_RESET), out);
    n_printed += fprintf(out, " %s ", start_name);
    pad = n_printed;

    /* print only simple usage summary if requested */
    if ((opt_config->flags & OPT_FLAG_SIMPLEUSAGE) != 0) {
        fprintf(out, "[options] [arguments]");
        opt_newline(out, opt_config, 1);
        return ;
    }
    if (max_optlen < sizeof(OPT_USAGE_SUMUP_END_DESC) - 1) {
        max_optlen = sizeof(OPT_USAGE_SUMUP_END_DESC) - 1;
    }
    /* reduce alignment if screen is too smal */
    if (n_printed + 4 /* '[-X]' */ + max_optlen > max_columns) {
        pad = 2;
        n_printed = 0;
        opt_newline(out, opt_config, 1);
        n_printed += fprintf(out, "  ");
    }

    /* first pass to print short options without arguments */
    i_opt = 0;
    for (const opt_options_desc_t * opt = opt_config->opt_desc; !is_opt_end(opt); ++opt, ++i_opt) {
        if (opt->arg == NULL && is_valid_short_opt(opt->short_opt)
        &&  opt_alias(i_opt, opt_config) < 0) {
            if (n_printed + 2 > max_columns) {
                if (n_printed > pad)
                    fputc(']', out);
                opt_newline(out, opt_config, 1);
                for (n_printed = 0; n_printed < pad; ++n_printed)
                    fputc(' ', out);
            }
            if (n_printed == pad)
                n_printed += fprintf(out, "[-");
            vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_short);
            if (fputc(opt->short_opt, out) != EOF)
                n_printed++;
            fputs(vterm_color(fd, VCOLOR_RESET), out);
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
                /* will printf additional " [--<long-option>[=value]] [--]" string */
                if (n_printed + sizeof(OPT_USAGE_SUMUP_END_DESC) - 1 > max_columns) {
                    opt_newline(out, opt_config, 1);
                    for (n_printed = 1; n_printed < pad; ++n_printed)
                        fputc(' ', out);
                }
                n_printed += fprintf(out, OPT_USAGE_SUMUP_END_DESC1);
                vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_uselong);
                n_printed += fprintf(out, OPT_USAGE_SUMUP_END_DESC2);
                fputs(vterm_color(fd, VCOLOR_RESET), out);
                n_printed += fprintf(out, OPT_USAGE_SUMUP_END_DESC3);
            }
            /* check columns limit */
            if (n_printed + len > max_columns) {
                opt_newline(out, opt_config, 1);
                for (n_printed = 1; n_printed < pad; ++n_printed)
                    fputc(' ', out);
            }
            /* display it */
            if (isarg) {
                i_firstarg = i_opt;
                n_printed += fprintf(out, " %s", opt->arg);
            } else if (*opt->arg != '[') {
                n_printed += fprintf(out, " [-");
                vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_useshort);
                if (fputc(opt->short_opt, out) != EOF)
                    n_printed++;
                fputs(vterm_color(fd, VCOLOR_RESET), out);
                n_printed += fprintf(out, "<%s>]", opt->arg);
            } else {
                n_printed += fprintf(out, " [-");
                vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_useshort);
                if (fputc(opt->short_opt, out) != EOF)
                    n_printed++;
                fputs(vterm_color(fd, VCOLOR_RESET), out);
                n_printed += fprintf(out, "%s]", opt->arg);
            }
        }
    }
    /* print " [--<long-option>[=value]] [--]" if not already done */
    if (i_firstarg < 0) {
        if (n_printed + sizeof(OPT_USAGE_SUMUP_END_DESC) - 1 > max_columns) {
            opt_newline(out, opt_config, 1);
            for (n_printed = 1; n_printed < pad; ++n_printed)
                fputc(' ', out);
        }
        n_printed += fprintf(out, OPT_USAGE_SUMUP_END_DESC1);
        vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_uselong);
        n_printed += fprintf(out, OPT_USAGE_SUMUP_END_DESC2);
        fputs(vterm_color(fd, VCOLOR_RESET), out);
        n_printed += fprintf(out, OPT_USAGE_SUMUP_END_DESC3);
    }
    opt_newline(out, opt_config, 1);
}

#define OPT_IS_EOL(ch) ((ch) == '\n' \
                        || ((ch) == '\r' && filter != NULL))

int opt_usage(int exit_status, opt_config_t * opt_config, const char * filter) {
    char            desc_buffer[4096];
    FILE *          out = NULL;
    unsigned int    max_columns;
    int             fd, i_opt, i_current_section = -1, filter_matched = 0;
    unsigned int    desc_headsz, opt_headsz;
    unsigned int    max_optlen;
    int             desc_truncated = 0;

    /* sanity checks */
    LOG_DEBUG(g_vlib_log, "%s(): entering", __func__);
    fflush(NULL);
    if (opt_config == NULL || opt_config->argv == NULL || opt_config->opt_desc == NULL) {
        return opt_error(OPT_ERROR(OPT_EFAULT), opt_config, OPTERR_PRINT_ERR, NULL,
                         "%s(): opt_config or opt_desc or argv is NULL!\n", __func__);
    }

    /* exit if OPT_FLAG_SILENT */
    if ((opt_config->flags & OPT_FLAG_SILENT) != 0) {
        LOG_DEBUG(g_vlib_log, "%s(silent): leaving with status %d", __func__, exit_status);
        return exit_status;
    }

    /* detect if macro was used to initialize opt_config, use defaults if not */
    opt_check_opt_config(opt_config);

    desc_headsz = strlen(opt_config->desc_head);
    opt_headsz = strlen(opt_config->opt_head);

    /* choose the FILE* to be used as output */
    if (opt_config->log != NULL && opt_config->log->out != NULL) {
        out = opt_config->log->out;
        flockfile(out);
    }

    /* if this is an error: use stderr and put a blank between error message and usage */
    if (OPT_IS_ERROR(exit_status) != 0) {
        if (out == NULL) {
            out = stderr;
            flockfile(out);
        }
        opt_newline(out, opt_config, 0);
    } else if (out == NULL) {
        out = stdout;
        flockfile(out);
    }

    /* set fd to non-tty if OPT_FLAG_COLOR is OFF */
    fd = (opt_config->flags & OPT_FLAG_COLOR) != 0 ? fileno(out) : -1;

    /* get max columns usable for display */
    if ((i_opt = vterm_get_columns(fileno(out))) <= 0) {
        max_columns = 80; /* not a tty or error retrieving columns */
    } else {
        max_columns = i_opt;
    }
    if (opt_config->desc_minlen <= 3) {
        opt_config->desc_minlen = 4;
    }

    /* get estimation maximum length of options (without description)
     * it is not 100% accurate, but not critical as this is used to
     * determine if desc_align has to be reduced in order to respect desc_minlen. */
    if ((opt_config->flags & OPT_FLAG_MIN_DESC_ALIGN) != 0) {
        max_optlen = 0;
        i_opt = 0;
        i_current_section = -1;
        for (const opt_options_desc_t * opt = opt_config->opt_desc;
             !is_opt_end(opt); ++opt, ++i_opt) {
            if (!is_opt_section(opt->short_opt)) {
                unsigned int curlen = opt_headsz;
                if (is_valid_short_opt(opt->short_opt))
                    curlen += 2; /* '-X' */
                if (opt->long_opt != NULL) {
                    if (curlen > opt_headsz) curlen += 2; /* ', ' */
                    curlen += strlen(opt->long_opt) + 2; /* '--<long>' */
                }
                for (const opt_options_desc_t * opt2 = opt + 1; !is_opt_end(opt2); ++opt2) {
                    if (opt->short_opt == opt2->short_opt && opt2->long_opt != NULL) {
                        size_t opt2len = strlen (opt2->long_opt);
                        if (curlen + 2 /* ', ' */ + opt2len + 1
                            > 1 + (opt_config->desc_align))
                            curlen = opt_headsz;
                        else
                            curlen += 2; /* ', ' */
                        curlen += opt2len + 2; /* '--<long>' */
                    }
                }
                if (opt->arg != NULL) {
                    if (curlen > opt_headsz) ++curlen; /* ' ' */
                    curlen += strlen(opt->arg); /* '<arg> ' */
                }
                if (curlen > max_optlen
                && curlen < 1 + (opt_config->desc_align))
                    max_optlen = curlen;
            } else {
                i_current_section = i_opt;
                if (i_opt > 0 && filter == NULL
                && (opt_config->flags & OPT_FLAG_MAINSECTION) != 0)
                    break ;
            }
        }
        LOG_DEBUG(g_vlib_log == NULL || g_vlib_log->out == out ? NULL : g_vlib_log,
            "opt_usage: MAX_OPTLEN:%d", max_optlen);
        if (max_optlen == 0)
            max_optlen = opt_config->desc_align;

    } else
        max_optlen = opt_config->desc_align;

    /* print list of options with their descrption */
    i_opt = 0;
    i_current_section = -1;
    for (const opt_options_desc_t * opt = opt_config->opt_desc;
                                    !is_opt_end(opt); ++opt, ++i_opt) {
        unsigned int    n_printed = 0;
        size_t          len;
        size_t          desc_size = 0, * psize = NULL;
        const char *    token;
        int             eol_shift = 0;
        const char *    next;
        int             is_section;
        int             colors = 0;

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
        ++filter_matched;

        if (filter_matched == 1) {
            /* ON FIRST MATCH, initialize log header, adjust sizing parameters, print summary */
            /* if options are displayed through log, get header size, and reduce max_columns */
            if (opt_config->log != NULL && LOG_CAN_LOG(opt_config->log, OPT_USAGE_LOGLEVEL)) {
                len = log_header(OPT_USAGE_LOGLEVEL, opt_config->log, NULL, NULL, 0);
                if (isatty(fileno(out))) {
                    max_columns -= len;
                }
            }
            /* print program name, version and usage summary */
            opt_print_usage_summary(opt_config, out, max_columns, max_optlen);

            /* Check if requested min size of opts. descs. fits in max_columns. */
            if (max_columns < opt_headsz + 3 + opt_config->desc_minlen + desc_headsz) {
                /* columns below minimum: use minimum description alignment */
                max_optlen = opt_headsz + 3 /* '-C ' */ ;
            } else if (max_columns < max_optlen + opt_config->desc_minlen + desc_headsz) {
                /* columns above minimum: increase description alignment */
                max_optlen = max_columns - desc_headsz - opt_config->desc_minlen;
                //max_optlen = opt_headsz + 3 /* '-C ' */ ;
            }
        }

        if (opt_config->log != NULL && ! LOG_CAN_LOG(opt_config->log, OPT_USAGE_LOGLEVEL)) {
            continue ;
        }
        if (filter != NULL && filter_matched == 1) {
            /* add title "Filtered options", with \n if current section starts with \n */
            if (i_current_section != -1
            &&  *(opt_config->opt_desc[i_current_section].desc) == '\n') {
                opt_newline(out, opt_config, 1);
            }
            vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_sect);
            fprintf(out, "Filtered options:%s", vterm_color(fd, VCOLOR_RESET));
            /* add newline unless current desc is a section starting with \n */
            if (is_section == 0
            ||  *(opt_config->opt_desc[i_opt].desc) != '\n') {
                opt_newline(out, opt_config, 1);
            }
        }

        if (!is_section) {
            /* skip option if this is an alias */
            if (opt_alias(i_opt, opt_config) >= 0)
                continue;

            /* short options */
            n_printed += fwrite(opt_config->opt_head, sizeof(char), opt_headsz, out);
            if (is_valid_short_opt(opt->short_opt)) {
                if (fputc('-', out) != EOF)
                    n_printed++;
                vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_short);
                if (fputc(opt->short_opt, out) != EOF)
                    n_printed++;
                fputs(vterm_color(fd, VCOLOR_RESET), out);
            }
            /* long option */
            if (opt->long_opt != NULL) {
                if (n_printed > opt_headsz) {
                    n_printed += fwrite(", ", sizeof(char), 2, out);
                }
                if (fputs("--", out) != EOF)
                    n_printed += 2;
                vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_long);
                n_printed += fprintf(out, "%s", opt->long_opt);
                fputs(vterm_color(fd, VCOLOR_RESET), out);
            }
            /* look for long option aliases */
            for (const opt_options_desc_t *opt2 = opt + 1; !is_opt_end(opt2); ++opt2) {
                if (opt2->short_opt == opt->short_opt && opt2->long_opt != NULL
                && opt2->desc == NULL) {
                    len = strlen(opt2->long_opt);
                    if (n_printed > opt_headsz) {
                        n_printed += fprintf(out, ", ");
                    }
                    if (n_printed + len + opt_headsz + 2 /*', '*/ > max_columns) {
                        n_printed = opt_newline(out, opt_config, 1);
                        for (n_printed = 0; n_printed < opt_headsz; ++n_printed)
                            fputc(' ', out);
                    }
                    n_printed += fwrite("--", sizeof(char), 2, out);
                    vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_long);
                    n_printed += fwrite(opt2->long_opt, sizeof(char), len, out);
                    fputs(vterm_color(fd, VCOLOR_RESET), out);
                }
            }
            /* option argument name */
            if (opt->arg) {
                if (n_printed > opt_headsz && fputc(' ', out) != EOF)
                    n_printed++;
                vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_arg);
                n_printed += fprintf(out, "%s", opt->arg);
                fputs(vterm_color(fd, VCOLOR_RESET), out);
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
            opt_newline(out, opt_config, 1);
            continue ;
        }
        /* print EOL if characters printed exceed padding */
        if (n_printed > max_optlen) {
            n_printed = opt_newline(out, opt_config, 1);
        }
        /* color flag to colorize sections */
        colors = is_section && fd >= 0;
        /* parsing option descriptions, splitting them into words and fix alignment */
        int tmp_desc_truncated;
        while (1) {
            tmp_desc_truncated = 0;
            if ((len = strtok_ro_r(&token, " \n\r-,;:/?=+*&|\\", &next, psize,
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
            if ( ! OPT_IS_EOL(*token) && len + n_printed > max_columns) {
                if ((opt_config->flags & OPT_FLAG_TRUNC_COLS) != 0 && filter == NULL) {
                    tmp_desc_truncated = desc_truncated = 1;
                } else {
                    n_printed = opt_newline(out, opt_config, 1);
                    eol_shift = desc_headsz;
                }
            }
            /* Align description if needed */
            if (!is_section) {
                while (n_printed < max_optlen + eol_shift) {
                    fputc(' ', out);
                    n_printed++;
                }
                if (!eol_shift && fputs(opt_config->desc_head, out) != EOF)
                    n_printed += desc_headsz;
            }
            else if (colors && ( ! OPT_IS_EOL(*token))) {
                vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_sect);
                colors = 0xffff;
            }

            eol_shift = desc_headsz;
            if (tmp_desc_truncated
            || (OPT_IS_EOL(token[len-1]) && !is_section
                    && (opt_config->flags & OPT_FLAG_TRUNC_EOL) != 0 && filter == NULL)
            || ((opt_config->flags & OPT_FLAG_TRUNC_COLS) != 0 && filter == NULL
                && ( ((desc_size > 0) || (next && *next) ? 3 + 1 : 0)
                     + len + n_printed > max_columns ))) {
                while (len > 0 && --len + 3 /* ' **' */ + n_printed > max_columns)
                    ; /* loop */
                desc_truncated = tmp_desc_truncated = 1;
            }

            /* print description, replace '\r' by ' ' (summary mode) or '\n' (filter mode) */
            int cr;
            if ((cr = (len > 0 && token[len-1] == '\r')))
                --len;
            n_printed += fwrite(token, 1, len, out);
            if (cr) {
                n_printed += fputc(filter != NULL ? '\n' : ' ', out) != EOF ? 1 : 0;
                ++len;
            }

            /* reset color of section if set previously */
            if (colors == 0xffff && len > 0 && (OPT_IS_EOL(*token) || OPT_IS_EOL(token[len-1]))) {
                colors = 0;
                fputs(vterm_color(fd, VCOLOR_RESET), out);
            }

            if (tmp_desc_truncated) {
                break ;
            }

            if (OPT_IS_EOL(token[len-1]) || (cr && filter != NULL)) {
                n_printed = 0;
                if (opt_config->log != NULL)
                    log_header(OPT_USAGE_LOGLEVEL, opt_config->log, NULL, NULL, 0);
            }
        } /* ! while(1) -> split and print current option description */
        if (tmp_desc_truncated) {
            vterm_putcolor(fd < 0 ? NULL : out, opt_config->color_trunc);
            fputs(" **", out);
        }
        /* reset colors */
        if (fd >= 0) {
            fputs(vterm_color(fd, VCOLOR_RESET), out);
        }
        /* EOL before processing next option */
        opt_newline(out, opt_config, 1);
    } /* ! for (opt=...; !is_opt_end(); ...) */
    if (desc_truncated && opt_config->opt_help_name != NULL) {
        /* notice about description truncation and how to get full info */
        int is_long = *opt_config->opt_help_name && opt_config->opt_help_name[1];
        opt_newline(out, opt_config, 1);
        fprintf(out,
            "%s%s%s" "**%s" " use %s%s%s" "%s" "%s" "%s" "%s" "%s%s%s" "all%s "
            "or %s%s%s" "%s" "%s" "%s" "%s" "%s%s%s" "%s" "%s to see full usage description",
            OPT_COLOR_3ARGS(fd, opt_config->color_trunc), vterm_color(fd, VCOLOR_RESET),
            OPT_COLOR_3ARGS(fd, opt_config->color_long), is_long ? "--" : "-",
            opt_config->opt_help_name, is_long ? "=" : "",
            vterm_color(fd, VCOLOR_RESET),
            OPT_COLOR_3ARGS(fd, opt_config->color_arg), vterm_color(fd, VCOLOR_RESET),
            OPT_COLOR_3ARGS(fd, opt_config->color_long), is_long ? "--" : "-",
            opt_config->opt_help_name, is_long ? "=" : "",
            vterm_color(fd, VCOLOR_RESET),
            OPT_COLOR_3ARGS(fd, opt_config->color_arg),
            opt_config->opt_help_name, vterm_color(fd, VCOLOR_RESET));
    }
    /* display error if bad usage filter was given */
    if (filter != NULL && filter_matched == 0) {
        exit_status = OPT_ERROR(OPT_EBADFLT);
        opt_error(exit_status, opt_config,
            OPTERR_PRINT_ERR, NULL, "bad filter '%s%s%s%s%s'\n",
            OPT_COLOR_3ARGS(fd, opt_config->color_arg), filter, vterm_color(fd, VCOLOR_RESET));
    } else {
        opt_newline(out, opt_config, 0);
    }
    fflush(out);
    funlockfile(out);
    LOG_DEBUG(g_vlib_log, "%s(): leaving with status %d", __func__, exit_status);
    return exit_status;
}

int opt_parse_options(opt_config_t * opt_config) {
    const char *const*          argv;
    const opt_options_desc_t *  desc;
    int                         fd; /* fd for vterm_color() */

    /* sanity checks */
    LOG_DEBUG(g_vlib_log, "%s(): entering", __func__);
    if (opt_config == NULL || (desc = opt_config->opt_desc) == NULL
    ||  (argv = opt_config->argv) == NULL) {
        return opt_error(OPT_ERROR(OPT_EFAULT), opt_config, OPTERR_PRINT_ERR, NULL,
                         "%s(): opt_config or opt_desc or argv is NULL!\n", __func__);
    }

    /* initialize valgrind detection */
    vlib_thread_valgrind(opt_config->argc, argv);

    /* check opt_config and init it if needed */
    opt_check_opt_config(opt_config);

    if ((opt_config->flags & OPT_FLAG_COLOR) == 0
    ||  (opt_config->flags & OPT_FLAG_SILENT) != 0
    || ! LOG_CAN_LOG(opt_config->log, LOG_LVL_ERROR))
        fd = -1;
    else
        fd = fileno(opt_config->log && opt_config->log->out ? opt_config->log->out : stderr);

    /* Analysing each argument of commandline. */
    for(int i_argv = 1, stop_options = 0; i_argv < opt_config->argc; i_argv++) {
        int result;
        /* Check Options (starting with '-') */
        if (!stop_options && *argv[i_argv] == '-' && argv[i_argv][1]) {
            char            short_str[2]    = { '-', 0 };
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
                    return opt_error(OPT_ERROR(OPT_ELONG), opt_config,
                                     OPTERR_SHOW_USAGE | OPTERR_PRINT_ERR, NULL,
                                     "unknown option '--" "%s%s%s" "%s" "%s'\n",
                                     OPT_COLOR_3ARGS(fd, opt_config->color_long),
                                     argv[i_argv]+2, vterm_color(fd, VCOLOR_RESET));
                }
                long_opt = desc[i_opt].long_opt;
                /* Check alias */
                if ((result = opt_alias(i_opt, opt_config)) >= 0)
                    i_opt = result;
                /* Long option is found, the matching short_opt is in opt_val. */
                /* if (!is_opt_user(desc[i_opt].short_opt)
                 * &&  !is_valid_short_opt(desc[i_opt].short_opt)) { */
                if (desc[i_opt].short_opt == OPT_ID_END) {
                    return opt_error(OPT_ERROR(OPT_ELONGID), opt_config,
                                     OPTERR_SHOW_USAGE | OPTERR_PRINT_ERR, NULL,
                                     "bad 'short_opt' value for option '" "%s%s%s" "%s" "%s'\n",
                                     OPT_COLOR_3ARGS(fd, opt_config->color_long),
                                     long_opt, vterm_color(fd, VCOLOR_RESET));
                }
                short_opts = short_str;
            }

            /* Proceed each short option, or the long option once. */
            for (const char * popt = short_opts; *popt; popt++, opt_arg = NULL) {
                *short_str = *popt;
                /* Check if short option is reconized */
                if (long_opt == NULL && (i_opt = get_registered_short_opt(*popt, opt_config)) < 0) {
                    return opt_error(OPT_ERROR(OPT_ESHORT), opt_config,
                                     OPTERR_SHOW_USAGE | OPTERR_PRINT_ERR, NULL,
                                     "unknown option '-" "%s%s%s" "%c" "%s'\n",
                                     OPT_COLOR_3ARGS(fd, opt_config->color_short),
                                     *popt, vterm_color(fd, VCOLOR_RESET));
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
                    return opt_error(OPT_ERROR(OPT_EOPTNOARG), opt_config,
                            OPTERR_SHOW_USAGE | OPTERR_PRINT_ERR,
                            long_opt != NULL ? long_opt : short_str,
                            "missing argument '" "%s%s%s" "%s" "%s" "' for option "
                            "'-" "%s" "%s%s%s" "%s" "%s'\n",
                            OPT_COLOR_3ARGS(fd, opt_config->color_arg),
                            desc[i_opt].arg, vterm_color(fd, VCOLOR_RESET),
                            long_opt != NULL ? "-" : "",
                            OPT_COLOR_3ARGS(fd, long_opt != NULL
                                    ? opt_config->color_long : opt_config->color_short),
                            long_opt != NULL ? long_opt : short_str,
                            vterm_color(fd, VCOLOR_RESET));
                }
                /* Check presence of unexpected option argument */
                if (opt_arg != NULL && desc[i_opt].arg == NULL) {
                    return opt_error(OPT_ERROR(OPT_EOPTARG), opt_config,
                            OPTERR_SHOW_USAGE | OPTERR_PRINT_ERR,
                            long_opt != NULL ? long_opt : short_str,
                            "unexpected argument '" "%s%s%s" "%s" "%s" "' for option "
                            "'-" "%s" "%s%s%s" "%s" "%s'\n",
                            OPT_COLOR_3ARGS(fd, opt_config->color_arg),
                            opt_arg, vterm_color(fd, VCOLOR_RESET),
                            long_opt != NULL ? "-" : "",
                            OPT_COLOR_3ARGS(fd, long_opt != NULL
                                    ? opt_config->color_long : opt_config->color_short),
                            long_opt != NULL ? long_opt : short_str,
                            vterm_color(fd, VCOLOR_RESET));
                }
                /* Call the callback if any */
                if (opt_config->callback) {
                    result = opt_config->callback(desc[i_opt].short_opt,
                                                  opt_arg, &i_argv, opt_config);
                    if (OPT_IS_ERROR(result)) {
                        if (OPT_EXIT_CODE(result) == OPT_EBADFLT)
                            ; /* nothing error message printed in opt_usage() */
                        else
                            result = OPT_ERROR(OPT_EBADOPT);
                        return opt_error(result, opt_config,
                                OPTERR_SHOW_USAGE | OPTERR_PRINT_ERR,
                                long_opt != NULL ? long_opt : short_str,
                                "incorrect option '-" "%s" "%s%s%s" "%s" "%s'\n",
                                long_opt != NULL ? "-" : "",
                                OPT_COLOR_3ARGS(fd, long_opt != NULL
                                    ? opt_config->color_long : opt_config->color_short),
                                long_opt != NULL ? long_opt : short_str,
                                vterm_color(fd, VCOLOR_RESET));
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
                    return opt_error(OPT_ERROR(OPT_EBADARG), opt_config,
                            OPTERR_SHOW_USAGE | OPTERR_PRINT_ERR, NULL,
                            "incorrect argument '" "%s%s%s" "%s" "%s'\n",
                            OPT_COLOR_3ARGS(fd, opt_config->color_arg),
                            arg, vterm_color(fd, VCOLOR_RESET));
                }
	            if (OPT_IS_EXIT_OK(result)) {
		            return OPT_EXIT_OK(result);
	            }
            }
        }
    }
    return OPT_CONTINUE(1);
}

int opt_parse_options_2pass(opt_config_t * opt_config, opt_option_callback_t callback2) {
    int ret;

    if (opt_config == NULL) {
        return OPT_ERROR(OPT_EFAULT);
    }
    for (int i = 0; i < 2; i++) {
        if (i == 0) {
            opt_config->flags |= OPT_FLAG_SILENT;
        } else {
            if (opt_config->callback != NULL) {
                ret = opt_config->callback(OPT_ID_END, NULL, NULL, opt_config);
                if (!OPT_IS_CONTINUE(ret))
                    return ret;
            }
            opt_config->flags &= ~OPT_FLAG_SILENT;
            opt_config->callback = callback2;
        }
        ret = opt_parse_options(opt_config);
    }
    return ret;
}

int opt_describe_filter(int short_opt, const char * arg, int * i_argv,
                        const opt_config_t * opt_config) {
    int n = 0, ret;
    (void) short_opt;

    if (opt_config == NULL || i_argv == NULL || arg == NULL) {
        return OPT_ERROR(OPT_EFAULT);
    }
    n += (ret = snprintf((char *)arg, *i_argv - n, "- filter: 'all")) > 0 ? ret : 0;
    for (const opt_options_desc_t * opt = opt_config->opt_desc; opt->short_opt || opt->desc; ++opt) {
        if (is_opt_section(opt->short_opt)) {
            n += (ret = snprintf((char *)arg + n, *i_argv - n, ",%s", opt->arg)) > 0 ? ret : 0;
        }
    }
    n += (ret = snprintf((char *)arg + n, *i_argv - n,
                         ",<shortopt>,<longopt>,:<option-description>' (shell patterns allowed)"
                        )) > 0 ? ret : 0;
    *i_argv = n;
    return OPT_CONTINUE(1);
}

#define FILE_PATTERN        "/* #@@# FILE #@@# "
#define FILE_PATTERN_END    " \\*/*"

int opt_filter_source(FILE * out, const char * filter, ...) {
    va_list         valist;
    vdecode_fun_t getsource;

    va_start(valist, filter);
    if (filter == NULL) {
        while ((getsource = va_arg(valist, vdecode_fun_t)) != NULL) {
            getsource(out, NULL, 0, NULL);
        }
        va_end(valist);
        return OPT_EXIT_OK(0);
    }

    const char *        search          = FILE_PATTERN;
    size_t              searchsz        = sizeof(FILE_PATTERN) - 1;
    char *              line = NULL;
    void *              ctx = NULL;
    size_t              line_capacity = 0;
    ssize_t             n;
    //const size_t        bufsz           = s_opt_filter_bufsz;
    char *              pattern;
    size_t              patlen          = strlen(filter);
    int                 fnm_flag        = FNM_CASEFOLD;

    if (*filter == ':') {
        /* handle search in source content rather than on file names */
        search = "";
        searchsz = 0;
        pattern = malloc((patlen + 2) * sizeof(char));
        strncpy(pattern, filter + 1, patlen - 1);
        strcpy(pattern + patlen - 1, "");
    } else {
        /* build search pattern */
        if ((pattern = malloc(sizeof(char) * (patlen + sizeof(FILE_PATTERN_END)))) == NULL) {
            return OPT_ERROR(1);
        }
        str0cpy(pattern, filter, patlen + 1);
        str0cpy(pattern + patlen, FILE_PATTERN_END, sizeof(FILE_PATTERN_END));
        if (strchr(filter, '/') != NULL && strstr(filter, "**") == NULL) {
            fnm_flag |= FNM_PATHNAME | FNM_LEADING_DIR;
        }
    }

    /* process each getsource function of the '...' va_list */
    //fputc('\n', out);
    while ((getsource = va_arg(valist, vdecode_fun_t)) != NULL) {
        int     found = 0;

        while ((n = vdecode_getline_fun(&line, &line_capacity, 2*PATH_MAX, &ctx, getsource)) > 0) {
            if ((strncmp(line, search, searchsz)) == 0) {
                if (*filter == ':') strcpy(pattern + patlen -1, line[n-1] == '\n' ? "\n" : "");
                if ((size_t) n > searchsz) {
                    found = (fnmatch(pattern, line + searchsz, fnm_flag) == 0);
                } else {
                    found = 0;
                }
            }
            if (found)
                fprintf(out, "%s", line);
        }
    }

    va_end(valist);
    if (line)
        free(line);
    free(pattern);

    return OPT_EXIT_OK(0);
}

const char * vlib_get_version() {
    return OPT_VERSION_STRING(BUILD_APPNAME, APP_VERSION, "git:" BUILD_GITREV);
}

#ifndef APP_INCLUDE_SOURCE
# define APP_NO_SOURCE_STRING "\n/* #@@# FILE #@@# " BUILD_APPNAME "/* */\n" \
                              BUILD_APPNAME " source not included in this build.\n"
int vlib_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx) {
    return vdecode_buffer(out, buffer, buffer_size, ctx,
                          APP_NO_SOURCE_STRING, sizeof(APP_NO_SOURCE_STRING) - 1);
}
#endif

