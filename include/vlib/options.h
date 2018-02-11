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

/**
 * Option description, User will create a 0,NULL,NULL,NULL terminated array.
 * For a long option without corresponding char option, use for short_opt
 * a non printable character except 0 (man isgraph); eg: 1000, 1001, ...
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
 * @param opt the option (short_opt) to be treated or 0 if it is a simple program argument
 * @param arg the option argument if any or simple program argument (opt=0), or NULL.
 * @param i_argv the current argv index for 'opt'.
 *               this handler has reponsibility to shift i_argv if it uses arguments.
 * @param opt_config the option config data including argc,argc,user_data, ...
 * @return
 *   > 0 on SUCCESS, no exit required
 *   0   on SUCCESS, exit required
 *   < 0 on ERROR, exit required
 */
typedef int     (*opt_option_callback_t)(int opt, const char *arg, int *i_argv,
                                         const opt_config_t * opt_config);

/** Option configuration with argc,argv,callback,desc,user_data,... */
struct opt_config_s {
    int                         argc;
    const char *const*          argv;
    opt_option_callback_t       callback;
    const opt_options_desc_t *  opt_desc;
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
 *    version_string the program version (can be OPT_VERSION_STR defined below)
 *                   it can also contain EOL and/or copyright, description.
 *    user_data the specific user data to be given to callback
 * @return
 *  > 0 on SUCCESS, no exit required
 *  0 on SUCCESS, with exit required
 *  < 0 on ERROR, with exit required
 */
int             opt_parse_options(const opt_config_t * opt_config);

/**
 * Get vlib version
 */
const char * vlib_get_version();

/**
 * Get vlib source code
 * @return array of const char *, terminated by NULL.
 */
const char *const* vlib_get_source();

#ifdef __cplusplus
}
#endif

#endif /* !ifndef *_H */

