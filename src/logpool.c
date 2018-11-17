/*
 * Copyright (C) 2018 Vincent Sallaberry
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
 * log utilities: structure holding a pool of log_t, sharing their files.
 * To see the full git history, some of theses functions were before in log.c.
 */
#include <stdlib.h>
#include <string.h>

#include "vlib/logpool.h"
#include "vlib/avltree.h"
#include "vlib/util.h"

/** internal log_pool structure */
struct logpool_s {
    //hash_t *    files;
    avltree_t *     logs;
    avltree_t *     files;
};

int logpool_prefixfind(const void * vvalue, const void * vpool_data) {
    const char *    prefix  = (const char *) vvalue;
    const log_t *   log     = (const log_t *) vpool_data;

    if (prefix == NULL || log == NULL || log->prefix == NULL) {
        return 1;
    }
    return strcmp(prefix, log->prefix);
}

int logpool_prefixcmp(const void * vpool1_data, const void * vpool2_data) {
    const log_t * log1 = (const log_t *) vpool1_data;

    if (log1 == NULL || vpool2_data == NULL || log1->prefix) {
        return (vpool1_data == vpool2_data) ? 0 : 1;
    }

    return logpool_prefixfind(log1->prefix, vpool2_data);
}

logpool_t *         logpool_create_from_cmdline(
                        logpool_t *         pool,
                        const char *        log_levels,
                        const char *const*  modules) {
    // FIXME: work in progress, PARSING OK, using it is TODO
    if (log_levels == NULL) {
        return NULL;
    }
    //char            prefix[LOG_PREFIX_SZ];
    //char            log_path[PATH_MAX];
    //const char *    next_mod_lvl;
    //log_t       default_log = { .level = LOG_LVL_DEFAULT, .out = stderr, .prefix = "main", .flags = 0};
    (void)modules;

    /* Parse log levels string with strtok_ro_r/strcspn instead of strtok_r or strsep
     * as those cool libc functions change the token by replacing sep with 0 */
    //const char * next = log_levels;
    size_t maxlen;
    size_t len;
    const char * next_tok;
    const char * mod_name;
    const char * mod_lvl;
    const char * mod_file;
    for (const char *next = log_levels; next && *next; /* no_incr */) {
        /* Get the following LOG configuration separated by ',' used for next loop */
        maxlen = strtok_ro_r(&next_tok, ",", &next, NULL, 0);
        LOG_DEBUG_BUF(NULL, next_tok, maxlen, "log_line ");

        /* Get the Module Name that must be followed by '=' */
        len = strtok_ro_r(&mod_name, "=", &next_tok, &maxlen, 1);
        //fprintf(stderr, "'%c'(%d)\n", *next_tok, *next_tok);
        //if (maxlen == 0) { maxlen += len; next = mod_name; len = 0; }
        LOG_DEBUG_BUF(NULL, mod_name, len, "mod_name ");

        /* Get the Module Level that can be followed by '@' or end of string. */
        len = strtok_ro_r(&mod_lvl, "@", &next_tok, &maxlen, 0);
        LOG_DEBUG_BUF(NULL, mod_lvl, len, "mod_lvl ");

        //len = strtok_ro_r(&mod_file, "\0", &next_tok, &maxlen, 0);
        mod_file = next_tok;
        len = maxlen;
        LOG_DEBUG_BUF(NULL, mod_file, len, "mod_file ");
    }
    return pool;
}

void                logpool_free(
                        logpool_t *         pool) {
    if (pool != NULL) {
        avltree_free(pool->logs);
        avltree_free(pool->files);
    }
}

