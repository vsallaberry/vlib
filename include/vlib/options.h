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
 * Simple Command Line options management.
 */
#ifndef VLIB_OPTIONS_H
#define VLIB_OPTIONS_H

#include "vlib/log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPT_DESCRIBE_OPTION     0x40000000              /* mask for option dynamic description */
#define OPT_OPTION_FLAG_MIN     0x00020000              /* internal: first bit used for flags */
#define OPT_OPTION_FLAG_MASK    (OPT_OPTION_FLAG_MIN - 1)/* mask to get OPT value without flags */

#define OPT_ID_END              0                       /* NULL short_opt for end of desc array */

#define OPT_ID_USER             0x00010000              /* first id available for USER */
#define OPT_ID_USER_MAX         (OPT_ID_USER + 0xfff)   /* last USER id - 4096 IDs */

#define OPT_ID_SECTION          (OPT_ID_USER_MAX + 1)   /* id for usage section */
#define OPT_ID_SECTION_MAX      (OPT_ID_SECTION + 0xfff)/* last id for usage section - 4096 IDs */

#define OPT_ID_ARG              (OPT_ID_SECTION_MAX + 1)/* id of an argument without option */
#define OPT_ID_ARG_MAX          (OPT_ID_ARG + 0xfff)    /* last id for simple argument - 4096 IDs */

#if OPT_ID_ARG_MAX >= OPT_OPTION_FLAG_MIN \
 || OPT_ID_USER_MAX >= OPT_OPTION_FLAG_MIN \
 || OPT_ID_SECTION_MAX >= OPT_OPTION_FLAG_MIN
# error "OPT_ID_{ARG|USER|SECTION}_MAX >= OPT_OPTION_FLAG_MIN"
#endif

/** options error codes, compare OPT_EXIT_CODE(status) with following values: */
enum {
    OPT_EFAULT      = 101,  /* bad opt_config input */
    OPT_ESHORT      = 102,  /* unknown short option */
    OPT_EBADOPT     = 103,  /* option rejected by callback */
    OPT_EBADARG     = 104,  /* argument rejected by callback */
    OPT_ELONG       = 105,  /* unknown long option */
    OPT_ELONGID     = 106,  /* bad long option short_opt value */
    OPT_EOPTNOARG   = 107,  /* argument missing for option */
    OPT_EOPTARG     = 108,  /* unexpected argument for option */
    OPT_EBADFLT     = 109   /* bad usage filter : usage not displayed */
};

/**
 * Option description, User will create a { OPT_ID_END,NULL,NULL,NULL } terminated array,
 * eg: opt_option_desc_t desc[] = {{'h',"help",NULL,"show help"},{OPT_ID_END,NULL,NULL,NULL}};
 * short_opt:
 *   - a character for the short option (isascii() && isgraph())
 *   - OPT_ID_USER...OPT_ID_USER_MAX for for a long option without corresponding
 *     short option.
 *   - OPT_ID_ARG...OPT_ID_ARG_MAX for a simple argument description (for opt_usage only)
 *   - OPT_ID_SECTION...OPT_ID_SECTION_MAX for a section description, used to split usage
 *     into sections and for the --help=<filter>, can be used with opt_config.flag
 *     OPT_FLAG_SHORTUSAGE.
 *   - it is not mandatory to use unique IDs for OPT_ID_SECTION*, OPT_ID_ARG*, but this
 *     allows support of dynamic option usage description (OPT_DESCRIBE_OPTION).
 * long_opt:
 *   the long option (without heading '--') or NULL if none
 *   It is possible to define long-option aliases by repeating a line with same short_opt (id)
 *   with another long-option, a NULL desc and a NULL arg.
 * arg:
 *   NULL if no argument, '[name]' if optional, 'name' if mandatory.
 *   for a section (OPT_ID_SECTION*), it is the section filter string (--help=<filter>)
 * desc:
 *   the description of option, argument, or the title of section
 */
typedef struct {
    int             short_opt;
    const char *    long_opt;
    const char *    arg;
    const char *    desc;
} opt_options_desc_t;

/** declaration for later definition */
typedef struct opt_config_s opt_config_t;

/**
 * opt_option_callback_t() : handler for customized option management
 *
 * @param opt * the option (short_opt) to be treated
 *            * or OPT_ID_ARG if it is a simple program argument
 *            * or (opt|OPT_DESCRIBE_OPTION) to give dynamic information
 *              about usage of this option (used for --help - opt_usage())
 *              In this case, arg is the buffer and i_argv the in/out buffer size.
 *
 * @param arg NULL or the option argument or simple program argument (opt=OPT_ID_ARG).
 *            the way arg is given depends on the 'arg' value in opt_options_desc_t.
 *
 * @param i_argv the current argv index.
 *               if the callback uses a non-given argument (arg==NULL) or does not
 *               use the given argument(arg!=NULL), it has the reponsibility to
 *               update i_argv accordinally, otherwise update of i_argv is automatic.
 *               setting i_argv to argc will stop option parsing, without error.
 *
 * @param opt_config the option config data including argc,argc,user_data, ...
 *
 * @return status of option parsing
 *   generated with 'return OPT_ERROR(code), OPT_EXIT_OK(code), OPT_CONTINUE(code)'
 *   tested with:
 *     IS_CONTINUE(status) => SUCCESS, no exit required
 *     IS_EXIT_OK(status)  => SUCCESS, exit(OPT_EXIT_CODE(status)) required.
 *     IS_ERROR(status)    => ERROR, exit(OPT_EXIT_CODE(status)) required.
 */
typedef int     (*opt_option_callback_t)(int opt, const char *arg, int *i_argv,
                                         const opt_config_t * opt_config);
/** opt_config_flag_t */
typedef enum {
    OPT_FLAG_NONE           = 0,
    OPT_FLAG_SILENT         = 1 << 0,   /* don' print error messages or usage */
    OPT_FLAG_MAINSECTION    = 1 << 1,   /* show only main usage section by default */
    OPT_FLAG_SIMPLEUSAGE    = 1 << 2,   /* print simple usage summury */
    OPT_FLAG_NOUSAGE        = 1 << 3,   /* don't print usage summary */
    /* end */
    OPT_FLAG_DEFAULT = OPT_FLAG_NONE
} opt_config_flag_t;

/** Option configuration with argc,argv,callback,desc,user_data,... */
struct opt_config_s {
    int                         argc;
    const char *const*          argv;
    opt_option_callback_t       callback;
    const opt_options_desc_t *  opt_desc;
    opt_config_flag_t           flags;
    const char *                version_string;
    void *                      user_data;
    log_t *                     log;
};

/**
 * print program usage.
 * The user can print additionnal information after calling this function.
 */
int             opt_usage(int exit_status, const opt_config_t * opt_config,
                          const char * filter);

/**
 * opt_parse_options() : Main entry point for generic options parsing
 * @param opt_config the option configuration including:
 *    argc given by main
 *    argv given by main
 *    opt_desc, an array of options, terminated by { OPT_ID_END, NULL,....,NULL }
 *    callback the user specific function called for each argument.
 *    version_string the program version (can be OPT_VERSION_STRING defined below)
 *                   it can also contain EOL and/or copyright, description.
 *    user_data the specific user data to be given to callback
 * @return the parse status, which can be treated as below:
 *  if (OPT_IS_CONTINUE(status)) => SUCCESS,  no exit required
 *  if (OPT_IS_EXIT_OK(status))  => SUCCESS,  exit(OPT_EXIT_CODE(status)) required.
 *  if (OPT_IS_ERROR(status))    => on ERROR, exit(OPT_EXIT_CODE(status)) required.
 */
int             opt_parse_options(const opt_config_t * opt_config);

/* opt_parse_options_2pass() : same as opt_parse_options(), with 1 pass in silent
 * mode with opt_config->callback and second pass without silent mode with callback2,
 * opt_config->callback(OPT_ID_END, NULL, NULL, opt_config) is called before second pass,
 * if its return value is not OPT_IS_CONTINUE, second pass is not done. */
int             opt_parse_options_2pass(opt_config_t * opt_config,
                                        opt_option_callback_t callback2);

/** opt_describe_filter() : default function describing the filter of --help command-line
 * option to be called from opt_callback_t option handler - see OPT_DESCRIBE_OPTION */
int             opt_describe_filter(int short_opt, const char * arg, int * i_argv,
                                    const opt_config_t * opt_config);

/**
 * Get vlib version
 */
const char *    vlib_get_version();

/**
 * Get vlib source code
 * @return array of const char *, terminated by NULL.
 */
const char *const* vlib_get_source();

/**
 * opt_parse_options() and opt_option_callback_t()
 * Macros to generate return values from code and to test these return values.
 * OPT_IS_*, OPT_EXIT_CODE are written to not repeat 'code' if it is a call().
 */
#define OPT_EXIT_OK(code)       0
#define OPT_ERROR(code)         (!(code) ? -1 : ((code) > 0 ? -(code) : (code)))
#define OPT_CONTINUE(code)      (!(code) ? 1  : ((code) < 0 ? -(code) : (code)))
#define OPT_IS_EXIT_OK(code)    ((code) == 0)
#define OPT_IS_ERROR(code)      ((code) < 0)
#define OPT_IS_CONTINUE(code)   ((code) > 0)
#define OPT_IS_EXIT(code)       ((code) <= 0)
#define OPT_EXIT_CODE(code)     (-(code))

/**
 * Default version string, and copyright notice.
 */
#define OPT_VERSION_STRING(app_name, app_version, revision) \
    app_name " v" app_version " " BUILD_APPRELEASE " (build:" \
    __DATE__ ", " __TIME__ " " revision ")"

#define OPT_LICENSE_GPL(author, copyright, gplver_s, gplver_l ) \
    "Copyright (C) " copyright " " author ".\n" \
    "License GPLv" gplver_s ": GNU GPL version " gplver_l " <http://gnu.org/licenses/gpl.html>.\n" \
    "This is free software: you are free to change and redistribute it.\n" \
    "There is NO WARRANTY, to the extent permitted by law."

#define OPT_LICENSE_GPL3PLUS(author, copyright) \
    OPT_LICENSE_GPL(author, copyright, "3+", "3 or later")

#define OPT_VERSION_STRING_LIC(app_name, app_version, revision, license) \
            OPT_VERSION_STRING(app_name, app_version, revision) "\n\n" license

#define OPT_VERSION_STRING_GPL3PLUS(app_name, app_version, revision, author, copyright) \
            OPT_VERSION_STRING_LIC(app_name, app_version, revision, \
                                   OPT_LICENSE_GPL3PLUS(author, copyright))

#ifdef __cplusplus
}
#endif

#endif /* !ifndef *_H */

