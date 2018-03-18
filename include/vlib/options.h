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

#ifdef __cplusplus
extern "C" {
#endif

#define OPT_DESCRIBE_OPTION     0x40000000      /* mask for option dynamic description */
#define OPT_OPTION_FLAG_MIN     0x00020000      /* internal: first bit used for flags */
#define OPT_ID_ARG              0               /* id of an argument without option */

#define OPT_ID_USER             0x00010000      /* first id available for USER */
#define OPT_ID_USER_MAX         (OPT_OPTION_FLAG_MIN-1) /* last available USER id  */

/**
 * Option description, User will create a { 0,NULL,NULL,NULL } terminated array.
 * For a long option without corresponding char option, use for short_opt
 * a non printable character (man isgraph) except 0, in the
 * range OPT_ID_USER..OPT_ID_USER_MAX.
 * For 'arg' : NULL if no argument, '[name]' if optional, 'name' if mandatory.
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
    OPT_FLAG_NONE = 0,
    OPT_FLAG_SILENT = 1 << 0,   /* don' print error messages or usage */
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
};

/**
 * print program usage.
 * The user can print additionnal information after calling this function.
 */
int             opt_usage(int exit_status, const opt_config_t * opt_config);

/**
 * opt_parse_options() : Main entry point for generic options parsing
 * @param opt_config the option configuration including:
 *    argc given by main
 *    argv given by main
 *    opt_desc, an array of options, terminated by { 0, NULL,....,NULL }
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

