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
#include <limits.h>

#include "vlib/logpool.h"
#include "vlib/avltree.h"
#include "vlib/util.h"
#include "vlib_private.h"

/* ************************************************************************ */

/** internal log_pool structure */
struct logpool_s {
    avltree_t *         logs;
    avltree_t *         files;
    pthread_rwlock_t    rwlock;
};

/** internal file structure (data of logpool->files) */
typedef struct {
    char *      path;
    FILE *      file;
    int         use_count;
} logpool_file_t;

/** internal logpool entry (data of logpool->logs) */
typedef struct {
    log_t               log;
    logpool_file_t *    file;
} logpool_entry_t;

/* ************************************************************************ */
static log_t *      logpool_add_unlocked(
                        logpool_t *         pool,
                        log_t *             log,
                        const char *        path);

/* ************************************************************************ */
int logpool_prefixcmp(const void * vpool1_data, const void * vpool2_data) {
    const logpool_entry_t * log1 = (const logpool_entry_t *) vpool1_data;
    const logpool_entry_t * log2 = (const logpool_entry_t *) vpool2_data;

    if (log1 == NULL || log2 == NULL) {
        if (log1 == log2) {
            return 0;
        }
        if (log1 == NULL) {
            return -1;
        }
        return 1;
    }
    if (log1->log.prefix == NULL || log2->log.prefix == NULL) {
        if (log1->log.prefix == log2->log.prefix) {
            return 0;
        }
        if (log1->log.prefix == NULL) {
            return -1;
        }
        return 1;
    }
    return strcmp(log1->log.prefix, log2->log.prefix);
}

/* ************************************************************************ */
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

/* ************************************************************************ */
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

/* ************************************************************************ */
static void logpool_file_free(void * vfile) {
    logpool_file_t * pool_file = (logpool_file_t *) vfile;

    if (pool_file != NULL) {
        if (pool_file->file && pool_file->file != stderr && pool_file->file != stdout) {
            fclose(pool_file->file);
        } else {
            fflush(pool_file->file);
        }
        if (pool_file->path != NULL) {
            free(pool_file->path);
        }
        free(pool_file);
    }
}

/* ************************************************************************ */
static void logpool_entry_free(void * ventry) {
    logpool_entry_t * logentry = (logpool_entry_t *) ventry;

    if (logentry != NULL) {
        if (&(logentry->log) == g_vlib_log) {
            log_set_vlib_instance(NULL);
        }
        log_destroy(&logentry->log);
        free(logentry);
    }
}

/* ************************************************************************ */
logpool_t *         logpool_create() {
    logpool_t * pool    = malloc(sizeof(logpool_t));
    log_t       log     = { LOG_LVL_INFO, stderr, LOG_FLAG_DEFAULT, NULL };

    if (pool == NULL) {
        return NULL;
    }
    if (pthread_rwlock_init(&pool->rwlock, NULL) != 0) {
        LOG_ERROR(g_vlib_log, "error pthread_rwlock_init(): %s", strerror(errno));
        free(pool);
        return NULL;
    }
    if (NULL == (pool->logs = avltree_create(AFL_DEFAULT | AFL_SHARED_STACK
                                             | AFL_INSERT_REPLACE | AFL_REMOVE_NOFREE,
                                             logpool_prefixcmp, logpool_entry_free))
    ||  NULL == (pool->files = avltree_create((AFL_DEFAULT & ~AFL_SHARED_STACK) | AFL_INSERT_NODOUBLE,
                                              logpool_pathcmp, logpool_file_free))) {
        LOG_ERROR(g_vlib_log, "error avltree_create(logs | files) : %s", strerror(errno));
        if (pool->logs != NULL) {
            avltree_free(pool->logs);
        }
        pthread_rwlock_destroy(&pool->rwlock);
        free(pool);
        return NULL;
    }
    /* use the same stack for logs and files */
    pool->files->stack = pool->logs->stack;

    /* add a default log instance */ //TODO
    logpool_add_unlocked(pool, &log, NULL);

    return pool;
}

/* ************************************************************************ */
void                logpool_free(
                        logpool_t *         pool) {
    if (pool != NULL) {
        pthread_rwlock_wrlock(&pool->rwlock);
        avltree_free(pool->files);
        avltree_free(pool->logs); /* must be last : it will free the rbuf stack */
        pthread_rwlock_unlock(&pool->rwlock);
        pthread_rwlock_destroy(&pool->rwlock);
        memset(pool, 0, sizeof(*pool));
        free(pool);
    }
}

/* ************************************************************************ */
logpool_t *         logpool_create_from_cmdline(
                        logpool_t *         pool,
                        const char *        log_levels,
                        const char *const*  modules) {
    (void)modules; //FIXME
    size_t          maxlen;
    size_t          len;
    const char *    next_tok;
    char *          token;
    log_t           log;
    size_t          argsz = PATH_MAX;
    char *          arg;

    /* sanity checks and initializations */
    if (pool == NULL && (pool = logpool_create())) {
        LOG_ERROR(g_vlib_log, "error: cannot create logpool: %s", strerror(errno));
        return NULL;
    }
    if (log_levels == NULL) {
        return pool;
    }
    if ((arg = malloc(argsz * sizeof(char))) == NULL) {
        LOG_ERROR(g_vlib_log, "error: cannot malloc buffer for log level parsing: %s.",
                              strerror(errno));
        return pool;
    }
    /* acquire the lock */
    pthread_rwlock_wrlock(&pool->rwlock);

    /* Parse log levels string with strtok_ro_r/strcspn instead of strtok_r or strsep
     * as those cool libc functions change the token by replacing sep with 0 */
    for (const char *next = log_levels; next && *next; /* no_incr */) {
        char            sep;
        const char *    mod_file = NULL;

        /* Get the following LOG configuration separated by ',' used for next loop */
        maxlen = strtok_ro_r(&next_tok, ",", &next, NULL, 0);
        LOG_DEBUG_BUF(g_vlib_log, next_tok, maxlen, "log_line ");

        /* copy line into a buffer, so that end-of-string can be added */
        if (maxlen + 1 > argsz) {
            char * new = realloc(arg, sizeof(char) * (maxlen + 1));
            if (new != NULL) {
                arg = new;
                argsz = maxlen + 1;
            } else {
                LOG_WARN(g_vlib_log, "warning: cannot realloc buffer for log level parsing: %s.",
                                     strerror(errno));
                maxlen = argsz - 1;
            }
        }
        strncpy(arg, next_tok, maxlen);
        arg[maxlen] = 0;
        next_tok = arg;

        /* Get the Module Name that must be followed by '=' */
        len = strtok_ro_r((const char **) &token, "=", &next_tok, &maxlen, 1);
        if (len > 0) {
            token[len] = 0;
            log.prefix = token;
        } else {
            log.prefix = "*";
        }
        LOG_DEBUG_BUF(g_vlib_log, token, len, "mod_name ");

        /* Get the Module Level that can be followed by '@', ':' or end of string. */
        len = strtok_ro_r((const char **) &token, "@:", &next_tok, &maxlen, 0);
        LOG_DEBUG_BUF(g_vlib_log, token, len, "mod_lvl ");
        sep = token[len];
        if (len > 0) {
            char * endptr = NULL;
            int level;
            token[len] = 0;
            errno = 0;
            level = strtol(token, &endptr, 0);
            if (errno != 0 || !endptr || *endptr != 0) {
                level = log_level_from_name(token);
                if (level < LOG_LVL_NB) {
                    log.level = level;
                } else {
                    LOG_WARN(g_vlib_log, "warning: unknown log level '%s'", token);
                    //FIXME return error ?
                }
            } else {
                log.level = level;
            }
        } else {
            log.level = LOG_LVL_INFO;
        }

        /* Get the the Log File */
        if (sep == '@') {
            len = strtok_ro_r((const char **) &token, ":", &next_tok, &maxlen, 0);
            LOG_DEBUG_BUF(g_vlib_log, token, len, "mod_file ");
            if (len > 0) {
                token[len] = 0;
                log.out = NULL;
                mod_file = token;
            } else {
                log.out = stderr;
            }
        } else {
            LOG_DEBUG_BUF(g_vlib_log, token, 0, "mod_file ");
            log.out = stderr;
        }

        /* Get the Log flags */
        token = (char *) next_tok;
        len = maxlen;
        LOG_DEBUG_BUF(g_vlib_log, token, len, "mod_flags ");
        if (len > 0) {
            const char *    next_flag   = token;
            size_t          maxflaglen  = maxlen;
            log_flag_t      flag;

            token[len] = 0;
            log.flags = LOG_FLAG_NONE;
            while (maxflaglen > 0) {
                len = strtok_ro_r((const char **) &token, "|+", &next_flag, &maxflaglen, 0);
                LOG_DEBUG_BUF(g_vlib_log, token, 0, "mod_flag ");
                if (len > 0) {
                    token[len] = 0;
                    if ((flag = log_flag_from_name(token)) != LOG_FLAG_UNKNOWN) {
                        log.flags |= flag;
                    } else {
                        LOG_WARN(g_vlib_log, "warning: unknown log flag '%s'", token);
                        //FIXME: return error?
                    }
                }
            }
        } else {
            log.flags = LOG_FLAG_DEFAULT | LOGPOOL_FLAG_TEMPLATE;
        }

        /* add the log to the pool */
        if (logpool_add_unlocked(pool, &log, mod_file) == NULL) {
            LOG_ERROR(g_vlib_log, "error: logpool_add(pref:%s,lvl:%s,flg:%d,path:%s) error.",
                                  log.prefix ? log.prefix : "<null>",
                                  log_level_name(log.level),
                                  log.flags,
                                  mod_file ? mod_file : "<null>");
        }
        else {
            LOG_VERBOSE(g_vlib_log, "Log ADDED pref:<%s> lvl:%s flags:%x out=%p path:%s",
                        log.prefix, log_level_name(log.level),
                        log.flags, (void *) log.out, mod_file);
        }
    }
    pthread_rwlock_unlock(&pool->rwlock);
    free(arg);
    return pool;
}

/* ************************************************************************ */
log_t *             logpool_add(
                        logpool_t *         pool,
                        log_t *             log,
                        const char *        path) {
    log_t *     result;

    if (pool)
        pthread_rwlock_wrlock(&pool->rwlock);

    result = logpool_add_unlocked(pool, log, path);

    if (pool)
        pthread_rwlock_unlock(&pool->rwlock);

    return result;
}

/* ************************************************************************ */
static log_t *      logpool_add_unlocked(
                        logpool_t *         pool,
                        log_t *             log,
                        const char *        path) {
    logpool_file_t      tmpfile, * pfile;
    logpool_entry_t *   logentry, * preventry;
    char                tmppath[20];

    if (pool == NULL || log == NULL) {
        return NULL;
    }
    if ((logentry = malloc(sizeof(logpool_entry_t))) == NULL) {
        return NULL;
    }
    /* copy the log data to the allocated logentry. prefix is duplicated,
     * and the log_destroy will not free the log or close the file */
    memcpy(&logentry->log, log, sizeof(logentry->log));
    logentry->log.flags &= ~(LOG_FLAG_CLOSEFILE | LOG_FLAG_FREELOG);
    logentry->log.flags |= LOG_FLAG_FREEPREFIX;
    if (log->prefix != NULL) {
        logentry->log.prefix = strdup(log->prefix);
    }
    logentry->file = NULL;

    /* insert logentry and get previous one if any */
    if ((preventry = avltree_insert(pool->logs, logentry)) == NULL) {
        logpool_entry_free(logentry);
        return NULL;
    }
    /* if path not given, use ';fd;' as path, else use given path. */
    if (path == NULL) {
        snprintf(tmppath, sizeof(tmppath), ";%d;", log->out ? fileno(log->out) : -1);
        path = tmppath;
    }
    /* look for already openned file */
    tmpfile.path = (char *) path;
    tmpfile.file = log->out;
    if ((pfile = avltree_find(pool->files, &tmpfile)) == NULL) {
        pfile = logpool_file_create(tmpfile.path, tmpfile.file);
        if (pfile == NULL || avltree_insert(pool->files, pfile) == NULL) {
            if (preventry != logentry) {
                if (--preventry->file->use_count == 0) {
                    avltree_remove(pool->files, preventry->file);
                }
                logpool_entry_free(preventry);
            }
            logpool_file_free(pfile);
            avltree_remove(pool->logs, log);
            logpool_entry_free(logentry);
            return NULL;
        }
    }
    /* checks whether the logentry has replaced a previous one */
    if (preventry != logentry) {
        LOG_DEBUG(g_vlib_log, "LOGENTRY <%s> REPLACED by <%s>",
                  preventry->log.prefix, logentry->log.prefix);
        if (preventry->file != pfile) { /* pointer comparison OK as doubles are forbiden. */
            /* the new logentry has a different file */
            LOG_DEBUG(g_vlib_log, "LOGENTRY REPLACED: FILE <%s> replaced by <%s>",
                      preventry->file->path, pfile->path);
            if (--preventry->file->use_count == 0) {
                LOG_DEBUG(g_vlib_log, "LOGENTRY REPLACED: previous file <%s> is NO LONGER USED",
                          preventry->file->path);
                /* the file of previous logentry is no longer used -> remove it. */
                avltree_remove(pool->files, preventry->file);
            }
        } else {
            LOG_DEBUG(g_vlib_log, "LOGENTRY REPLACED: SAME FILE <%s>", pfile->path);
            /* new log has the same file as previous: decrement because ++use_count later. */
            --preventry->file->use_count;
        }
        logpool_entry_free(preventry);
    }
    /* finish logentry initialization and return */
    logentry->file = pfile;
    logentry->log.out = pfile->file;
    ++pfile->use_count;
    log = &(logentry->log);
    return log;
}

/* ************************************************************************ */
int                 logpool_remove(
                        logpool_t *         pool,
                        log_t *             log) {
    logpool_entry_t *   logentry;

    if (pool == NULL || log == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&pool->rwlock);
    //FIXME: don't remove templates
    if ((logentry = avltree_remove(pool->logs, log)) != NULL) {
        if (logentry->file != NULL && --logentry->file->use_count == 0) {
            avltree_remove(pool->files, logentry->file);
        }
        logpool_entry_free(logentry);
    }
    pthread_rwlock_unlock(&pool->rwlock);

    return logentry == NULL ? -1 : 0;
}

/* ************************************************************************ */
log_t *             logpool_find(
                        logpool_t *         pool,
                        const char *        prefix) {
    log_t *             result = NULL;
    logpool_entry_t *   entry;
    log_t               ref;

    if (pool == NULL) {
        return NULL;
    }

    pthread_rwlock_rdlock(&pool->rwlock);

    ref.prefix = (char *) prefix;
    if ((entry = avltree_find(pool->logs, &ref)) != NULL) {
        result = &(entry->log);
    }

    pthread_rwlock_unlock(&pool->rwlock);

    return result;
}

/* ************************************************************************ */
log_t *             logpool_getlog(
                        logpool_t *         pool,
                        const char *        prefix,
                        int                 flags) {
    log_t *             result = NULL;
    logpool_entry_t *   entry;
    log_t               ref;

    if (pool == NULL) {
        return NULL;
    }
    ref.prefix = (char *) prefix;

    //TODO: think about optimization here to acquire only a read lock in some cases
    if ((flags & LPG_TRUEPREFIX) == 0) {
        pthread_rwlock_rdlock(&pool->rwlock);
    } else {
        pthread_rwlock_wrlock(&pool->rwlock);
    }

    /* look for the requested log instance */
    if ((entry = avltree_find(pool->logs, &ref)) == NULL) {
        if ((flags & LPG_NODEFAULT) != 0) {
            /* don't use default if flag forbids it */
            pthread_rwlock_unlock(&pool->rwlock);
            return NULL;
        }
        /* look for a default log instance */
        ref.prefix = NULL;
        entry = avltree_find(pool->logs, &ref);
    }
    if (entry != NULL) {
        result = &(entry->log);
    }

    if (entry != NULL && (flags & LPG_TRUEPREFIX) != 0
    &&  result->prefix != prefix
    &&  (result->prefix == NULL || prefix == NULL || strcmp(result->prefix, prefix))) {
        /* duplicate log and put requested prefix */
        memcpy(&ref, result, sizeof(ref));
        ref.prefix = (char *) prefix;
        result = logpool_add_unlocked(pool, &ref, entry->file->path);
    }

    pthread_rwlock_unlock(&pool->rwlock);

    return result;
}

/* ************************************************************************ */
static avltree_visit_status_t   logpool_filesz_visit(
                                    avltree_t *                         tree,
                                    avltree_node_t *                    node,
                                    const avltree_visit_context_t *     context,
                                    void *                              user_data) {
    logpool_file_t *    file = (logpool_file_t *) node->data;
    size_t *            psz = (size_t *) user_data;
    (void) tree;
    (void) context;

    if (psz != NULL && file != NULL) {
        *psz += (file->path != NULL ? strlen(file->path) : 0);// + sizeof(FILE);
    }
    return AVS_CONTINUE;
}
/* ************************************************************************ */
static avltree_visit_status_t   logpool_logsz_visit(
                                    avltree_t *                         tree,
                                    avltree_node_t *                    node,
                                    const avltree_visit_context_t *     context,
                                    void *                              user_data) {
    logpool_entry_t *   entry = (logpool_entry_t *) node->data;
    size_t *            psz = (size_t *) user_data;
    (void) tree;
    (void) context;

    if (psz != NULL && entry != NULL && entry->log.prefix != NULL) {
        *psz += strlen(entry->log.prefix);
    }
    return AVS_CONTINUE;
}
/* ************************************************************************ */
size_t              logpool_memorysize(
                        logpool_t *         pool) {
    size_t  datas_size = 0;

    if (pool == NULL) {
        errno = EINVAL;
        return 0;
    }
    LOG_VERBOSE(g_vlib_log, "LOGPOOL nbr of files : %zu", avltree_count(pool->files));
    LOG_VERBOSE(g_vlib_log, "LOGPOOL nbr of logs  : %zu", avltree_count(pool->logs));

    datas_size += avltree_count(pool->files) * sizeof(logpool_file_t);
    datas_size += avltree_count(pool->logs)  * sizeof(logpool_entry_t);

    avltree_visit(pool->files, logpool_filesz_visit, &datas_size, AVH_PREFIX);
    avltree_visit(pool->logs,  logpool_logsz_visit,  &datas_size, AVH_PREFIX);

    return sizeof(logpool_t)
           + avltree_memorysize(pool->logs)
           + avltree_memorysize(pool->files)
           + datas_size;
}

/* ************************************************************************ */
