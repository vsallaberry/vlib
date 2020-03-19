/*
 * Copyright (C) 2018-2020 Vincent Sallaberry
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
logpool_t * g_vlib_logpool = NULL;

/** internal logpool flags */
typedef enum {
    LPP_NONE            = 0,
    LPP_SILENT          = 1 << 0
} logpool_priv_flag_t;

/** internal log_pool structure */
struct logpool_s {
    avltree_t *         logs;
    avltree_t *         files;
    pthread_rwlock_t    rwlock;
    unsigned int        flags;
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

    if (path != NULL && file == NULL) {
        pool_file->file = fopen(path, "a");
    } else {
        pool_file->file = file;
    }

    if (path != NULL) {
        pool_file->path = strdup(path);
    } else {
        pool_file->path = NULL;
    }

    return pool_file;
}

/* ************************************************************************ */
static void logpool_file_free(void * vfile) {
    logpool_file_t * pool_file = (logpool_file_t *) vfile;

    if (pool_file != NULL) {
        if (pool_file->file != NULL) {
            if (pool_file->file != stderr && pool_file->file != stdout)
                fclose(pool_file->file);
            else
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
    log_t       log     = { LOG_LVL_INFO, LOG_FLAG_DEFAULT, stderr, NULL };

    if (pool == NULL) {
        return NULL;
    }
    if (pthread_rwlock_init(&pool->rwlock, NULL) != 0) {
        LOG_ERROR(g_vlib_log, "error pthread_rwlock_init(): %s", strerror(errno));
        free(pool);
        return NULL;
    }
    if (NULL == (pool->logs = avltree_create(AFL_DEFAULT | AFL_SHARED_STACK
                                             | AFL_INSERT_IGNDOUBLE | AFL_REMOVE_NOFREE,
                                             logpool_prefixcmp, logpool_entry_free))
    ||  NULL == (pool->files = avltree_create((AFL_DEFAULT & ~AFL_SHARED_STACK)
                                              | AFL_INSERT_NODOUBLE | AFL_REMOVE_NOFREE,
                                              logpool_pathcmp, logpool_file_free))) {
        LOG_ERROR(g_vlib_log, "error avltree_create(logs | files) : %s", strerror(errno));
        if (pool->logs != NULL) {
            avltree_free(pool->logs);
        }
        pthread_rwlock_destroy(&pool->rwlock);
        free(pool);
        return NULL;
    }
    pool->flags = LPP_NONE;

    /* use the same stack for logs and files */
    pool->files->stack = pool->logs->stack;

    /* add a default log instance */ //TODO
    logpool_add_unlocked(pool, &log, NULL);

    if (g_vlib_logpool == NULL) {
        g_vlib_logpool = pool;
    }

    return pool;
}

/* ************************************************************************ */
void                logpool_free(
                        logpool_t *         pool) {
    if (pool != NULL) {
        pthread_rwlock_wrlock(&pool->rwlock);
        if (pool == g_vlib_logpool) {
            g_vlib_logpool = NULL;
        }
        avltree_free(pool->files);
        avltree_free(pool->logs); /* must be last : it will free the rbuf stack */
        pthread_rwlock_unlock(&pool->rwlock);
        pthread_rwlock_destroy(&pool->rwlock);
        memset(pool, 0, sizeof(*pool));
        free(pool);
    }
}

/* ************************************************************************ */
static avltree_visit_status_t   logpool_enable_visit(
                                    avltree_t *                         tree,
                                    avltree_node_t *                    node,
                                    const avltree_visit_context_t *     context,
                                    void *                              user_data) {
    log_t *             log;
    int                 enable  = (int)((unsigned long)user_data);
    (void) context;

    if (tree == NULL)
        log = (log_t *) node; /* for calling this function outside of visit */
    else
        log = &(((logpool_entry_t *) node->data)->log); /* normal avltree_visit */

    if (log != NULL) {
        FILE * out = log_getfile_locked(log);

        if (enable == 0) {
            log->flags |= LOG_FLAG_SILENT;
        } else if (log != g_vlib_log || tree == NULL) {
            log->flags &= ~LOG_FLAG_SILENT;
        }

        if (out != NULL) {
            fflush(out);
            funlockfile(out);
        }
    }
    return AVS_CONTINUE;
}
/* ************************************************************************ */
int                 logpool_enable(
                        logpool_t *         pool,
                        log_t *             log,
                        int                 enable,
                        int *               prev_enable) {
    (void) log;

    if (pool == NULL) {
        if (prev_enable != NULL) {
            *prev_enable = g_vlib_log == NULL ? 1 : (g_vlib_log->flags & LOG_FLAG_SILENT) == 0;
        }
        logpool_enable_visit(NULL, (avltree_node_t *) g_vlib_log, NULL,
                             (void*)((unsigned long)enable));
    } else {
        pthread_rwlock_wrlock(&pool->rwlock);

        if (prev_enable != NULL) {
            *prev_enable = (pool->flags & LPP_SILENT) == 0;
        }

        if (enable == 0) {
            /* special case for vlib log to avoid avltree logging while disabling */
            logpool_enable_visit(NULL, (avltree_node_t *) g_vlib_log, NULL, (void*)(0UL));

            /* set SILENT flag to logpool to create new logs as SILENT */
            pool->flags |= LPP_SILENT;
        }

        avltree_visit(pool->logs, logpool_enable_visit, (void*)((unsigned long)enable), AVH_PREFIX);

        if (enable == 1) {
            logpool_enable_visit(NULL, (avltree_node_t *) g_vlib_log, NULL, (void*)(1UL));
            pool->flags &= ~LPP_SILENT;
        }

        pthread_rwlock_unlock(&pool->rwlock);
    }
    return 0;
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
    if (pool == NULL && (pool = logpool_create()) == NULL) {
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
            log.prefix = NULL;
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

    if (pool == NULL || log == NULL) {
        return NULL;
    }

    pthread_rwlock_wrlock(&(pool->rwlock));

    result = logpool_add_unlocked(pool, log, path);

    pthread_rwlock_unlock(&(pool->rwlock));

    return result;
}

/* ************************************************************************ */
static log_t *      logpool_add_unlocked(
                        logpool_t *         pool,
                        log_t *             log,
                        const char *        path) {
    char                abspath[PATH_MAX*2];
    logpool_file_t      tmpfile, * pfile;
    logpool_entry_t *   logentry, * preventry;

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
    if ((pool->flags & LPP_SILENT) != 0)
        logentry->log.flags |= LOG_FLAG_SILENT;
    logentry->file = NULL;

    /* if path not given, use ';fd;fileptr;' as path, else get absolute path from given path. */
    if (path == NULL) {
        if (logentry->log.out == NULL)
            logentry->log.out = stderr;
        snprintf(abspath, sizeof(abspath), ";%d;%08lx;",
                 fileno(logentry->log.out), (unsigned long) logentry->log.out);
    } else {
        logentry->log.out = NULL;
        if (*path == ';') {
            str0cpy(abspath, path, sizeof(abspath));
        } else {
            vabspath(abspath, sizeof(abspath), path, NULL);
        }
    }

    /* insert logentry and get previous one if any */
    if ((preventry = avltree_insert(pool->logs, logentry)) == NULL) {
        logpool_entry_free(logentry);
        return NULL;
    }

    /* look for already openned file */
    tmpfile.path = abspath;
    tmpfile.file = log->out;
    if ((pfile = avltree_find(pool->files, &tmpfile)) == NULL) {
        pfile = logpool_file_create(tmpfile.path, tmpfile.file);
        if (pfile == NULL || avltree_insert(pool->files, pfile) == NULL) { //TODO FIXME IGNDOUBLE
            LOG_WARN(g_vlib_log, "%s(): **** error case TODO FIXME ******", __func__);
            if (preventry != logentry) {
                if (--preventry->file->use_count == 0
                && avltree_remove(pool->files, preventry->file) != NULL) {
                    logpool_file_free(preventry->file);
                }
                logpool_entry_free(preventry);
            }
            logpool_file_free(pfile);
            avltree_remove(pool->logs, log);
            logpool_entry_free(logentry);
            return NULL;
        }
    }

    /* logentry initialization */
    logentry->file = pfile;
    logentry->log.out = pfile->file;

    /* checks whether the logentry was already in the pool and update it with new data */
    if (preventry != logentry) {
        logpool_file_t *    file_to_free = NULL;
        FILE *              logout;

        LOG_DEBUG(g_vlib_log, "LOGENTRY <%s> REPLACED by <%s>",
                  preventry->log.prefix, logentry->log.prefix);

        /* lock log file before continuing, to not disturb threads owning this log entry */
        logout = log_getfile_locked(&(preventry->log));

        if (preventry->file != pfile) { /* pointer comparison OK as doubles are forbiden. */
            /* the new logentry has a different file */
            LOG_DEBUG(g_vlib_log, "LOGENTRY REPLACED: FILE <%s> replaced by <%s>",
                      preventry->file->path, pfile->path);
            if (--preventry->file->use_count == 0) {
                LOG_DEBUG(g_vlib_log, "LOGENTRY REPLACED: previous file <%s> is NO LONGER USED",
                          preventry->file->path);
                /* the file of previous logentry is no longer used -> remove it. */
                if (avltree_remove(pool->files, preventry->file) == NULL) {
                    LOG_ERROR(g_vlib_log, "error: cannot remove file <%s> from logpool",
                              preventry->file->path);
                } else {
                    file_to_free = preventry->file;
                }
            }
        } else {
            LOG_DEBUG(g_vlib_log, "LOGENTRY REPLACED: SAME FILE <%s>", pfile->path);
            /* new log has the same file as previous: decrement because ++use_count below. */
            --preventry->file->use_count;
        }
        /* prefix of prev entry is kept, new one (equal) is freed */
        if (logentry->log.prefix != NULL) {
            free(logentry->log.prefix);
            logentry->log.prefix = preventry->log.prefix;
        }
        /* copy new log entry data into previous entry (data changes, log pointer does not */
        memcpy(preventry, logentry, sizeof(*logentry));

        /* we can now unlock the old file, and free it as updated log entry is ready to be used */
        funlockfile(logout);
        if (file_to_free != NULL)
            logpool_file_free(file_to_free);
        logentry->log.prefix = NULL; /* logentry->log.prefix already freed */
        logpool_entry_free(logentry);
    }

    /* finish initialization and return log instance */
    ++pfile->use_count;
    log = &(preventry->log);

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
    //TODO: don't remove templates
    if ((logentry = avltree_remove(pool->logs, log)) != NULL) {
        if (logentry->file != NULL && --logentry->file->use_count == 0
        && avltree_remove(pool->files, logentry->file) != NULL) {
            logpool_file_free(logentry->file);
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
static avltree_visit_status_t   logpool_logprint_visit(
                                    avltree_t *                         tree,
                                    avltree_node_t *                    node,
                                    const avltree_visit_context_t *     context,
                                    void *                              user_data) {
    logpool_entry_t *   entry = (logpool_entry_t *) node->data;
    log_t *             log = user_data != NULL ? (log_t *) user_data : g_vlib_log;
    (void) tree;
    (void) context;

    if (entry != NULL) {
        LOG_INFO(log, "LOGPOOL: ENTRY %-15s out:%08lx,fd:%02d file:%08lx,"
                "fd:%02d,used:%d,path:%s",
                entry->log.prefix ? entry->log.prefix : "(null)",
                (unsigned long) entry->log.out,
                entry->log.out ? fileno(entry->log.out) : -1,
                (unsigned long) (entry->file ? entry->file->file : NULL),
                entry->file && entry->file->file ? fileno(entry->file->file) : -1,
                entry->file ? entry->file->use_count : -1,
                entry->file && entry->file->path ? entry->file->path : "(null)");
    } else {
        LOG_INFO(log, "LOGPOOL: ENTRY (null)");
    }
    return AVS_CONTINUE;
}
/* ************************************************************************ */
static avltree_visit_status_t   logpool_fileprint_visit(
                                    avltree_t *                         tree,
                                    avltree_node_t *                    node,
                                    const avltree_visit_context_t *     context,
                                    void *                              user_data) {
    logpool_file_t *    file = (logpool_file_t *) node->data;
    log_t *             log = user_data != NULL ? (log_t *) user_data : g_vlib_log;
    (void) tree;
    (void) context;

    if (file != NULL) {
        LOG_INFO(log, "LOGPOOL: FILE %16s out:%08lx fd:%02d used:%d FILE <%s>",
                "", (unsigned long) (file->file),
                file->file ? fileno(file->file) : -1, file->use_count,
                file->path ? file->path : "(null)");
    } else {
        LOG_INFO(log, "LOGPOOL: FILE (null)");
    }

    return AVS_CONTINUE;
}
/* ************************************************************************ */
#if defined(_DEBUG)
static int avltree_print_logpool(FILE * out, const avltree_node_t * node) {
    logpool_entry_t *   entry = (logpool_entry_t *) node->data;
    char                name[13];
    const ssize_t       name_sz = sizeof(name) / sizeof(*name);
    ssize_t             len;

    len = snprintf(name, name_sz, "%s", entry->log.prefix);
    if (len < 0)  {
        str0cpy(name, "error", name_sz);
    }
    if (len >= name_sz) {
        strncpy(name + name_sz - 3, "..", 3);
    }
    fputs(name, out);
    return len;
}
#endif
/* ************************************************************************ */
int                 logpool_print(
                        logpool_t *         pool,
                        log_t *             log) {
    if (pool == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (log == NULL) {
        log = g_vlib_log;
    }

    LOG_INFO(log, "LOGPOOL nbr of files : %zu", avltree_count(pool->files));
    LOG_INFO(log, "LOGPOOL nbr of logs  : %zu", avltree_count(pool->logs));

    avltree_visit(pool->files, logpool_fileprint_visit, log, AVH_PREFIX);
    avltree_visit(pool->logs,  logpool_logprint_visit,  log, AVH_PREFIX);

#if defined(_DEBUG)
    if (log && LOG_CAN_LOG(log, LOG_LVL_SCREAM) && avltree_count(pool->logs) < 50) {
        avltree_print(pool->logs, avltree_print_logpool, log->out);
    }
#endif

    return 0;
}

/* ************************************************************************ */
