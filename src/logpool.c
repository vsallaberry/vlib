/*
 * Copyright (C) 2018-2019 Vincent Sallaberry
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
#include <pthread.h>
#include <errno.h>

#include "vlib/logpool.h"
#include "vlib/avltree.h"
#include "vlib/util.h"
#include "vlib_private.h"

/** internal log_pool structure */
struct logpool_s {
    avltree_t *         logs;
    avltree_t *         files;
    pthread_rwlock_t    rwlock;
};

/** internal file structure */
typedef struct {
    char *      path;
    FILE *      file;
    int         use_count;
} logpool_file_t;

/*
int logpool_prefixfind(const void * vvalue, const void * vpool_data) {
    const char *    prefix  = (const char *) vvalue;
    const log_t *   log     = (const log_t *) vpool_data;

    if (prefix == NULL || log == NULL || log->prefix == NULL) {
        return 1;
    }
    return strcmp(prefix, log->prefix);
} */

int logpool_prefixcmp(const void * vpool1_data, const void * vpool2_data) {
    const log_t * log1 = (const log_t *) vpool1_data;
    const log_t * log2 = (const log_t *) vpool2_data;

    if (log1 == NULL || log2 == NULL) {
        if (log1 == log2) {
            return 0;
        }
        if (log1 == NULL) {
            return -1;
        }
        return 1;
    }
    if (log1->prefix == NULL || log2->prefix == NULL) {
        if (log1->prefix == log2->prefix) {
            return 0;
        }
        if (log1->prefix == NULL) {
            return -1;
        }
        return 1;
    }
    return strcmp(log1->prefix, log2->prefix);
}

int logpool_pathcmp(const void * vpool1_data, const void * vpool2_data) {
    const logpool_file_t * file1 = (const logpool_file_t *) vpool1_data;
    const logpool_file_t * file2 = (const logpool_file_t *) vpool2_data;

    if (file1 == NULL || file2 == NULL) {
        if (file1 == file2) {
            return 0;
        }
        if (file1 == NULL) {
            return -1;
        }
        return 1;
    }
    if (file1->path == NULL || file2->path == NULL) {
        if (file1->path == file2->path) {
            return 0;
        }
        if (file1->path == NULL) {
            return -1;
        }
        return 1;
    }
    return strcmp(file1->path, file2->path);
}

static logpool_file_t * logpool_file_create(const char * path, FILE * file) {
    logpool_file_t * pool_file = malloc(sizeof(logpool_file_t));

    if (pool_file == NULL) {
        return NULL;
    }
    pool_file->use_count = 0;
    if (file != NULL) {
        pool_file->file = file;
    } else {
        pool_file->file = fopen(path, "a");
    }
    pool_file->path = strdup(path);

    return pool_file;
}

static void logpool_file_free(void * vfile) {
    logpool_file_t * pool_file = (logpool_file_t *) vfile;

    if (pool_file != NULL) {
        if (pool_file->file && pool_file->file != stderr && pool_file->file != stdout) {
            fclose(pool_file->file);
        }
        if (pool_file->path != NULL) {
            free(pool_file->path);
        }
        free(pool_file);
    }
}

logpool_t *         logpool_create() {
    logpool_t * pool = malloc(sizeof(logpool_t));

    if (pool == NULL) {
        return NULL;
    }
    if (pthread_rwlock_init(&pool->rwlock, NULL) != 0) {
        LOG_ERROR(g_vlib_log, "error pthread_rwlock_init(): %s", strerror(errno));
        free(pool);
        return NULL;
    }
    if (NULL == (pool->logs
                 = avltree_create(AFL_DEFAULT | AFL_SHARED_STACK, logpool_prefixcmp, log_destroy))
    ||  NULL == (pool->files = avltree_create(AFL_DEFAULT & ~AFL_SHARED_STACK,
                                              logpool_pathcmp, logpool_file_free))) {
        LOG_ERROR(g_vlib_log, "error avltree_create(logs | files) : %s", strerror(errno));
        if (pool->logs != NULL) {
            avltree_free(pool->logs);
        }
        free(pool);
    }
    /* use the same stack for logs and files */
    pool->files->stack = pool->logs->stack;

    return pool;
}

void                logpool_free(
                        logpool_t *         pool) {
    if (pool != NULL) {
        avltree_free(pool->logs);
        avltree_free(pool->files);
        pthread_rwlock_destroy(&pool->rwlock);
    }
}

logpool_t *         logpool_create_from_cmdline(
                        logpool_t *         pool,
                        const char *        log_levels,
                        const char *const*  modules) {
    // FIXME: work in progress, PARSING OK, using it is TODO
    pool = logpool_create();
    if (pool == NULL) {
        return NULL;
    }
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

int                 logpool_add(
                        logpool_t *         pool,
                        log_t *             log) {
    if (pool == NULL || log == NULL) {
        return -1;
    }
    //TODO
    return -1;
}

int                 logpool_remove(
                        logpool_t *         pool,
                        log_t *             log) {
    if (pool == NULL || log == NULL) {
        return -1;
    }
    //TODO
    return -1;
}

log_t *             logpool_find(
                        logpool_t *         pool,
                        const char *        prefix) {
    log_t *     result;
    log_t       ref;

    if (pool == NULL || prefix == NULL) {
        return NULL;
    }
    ref.prefix = (char *) prefix;
    result = avltree_find(pool->logs, &ref);
    return result;
}

