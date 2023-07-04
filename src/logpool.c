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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <fnmatch.h>
#include <sys/stat.h>

#include "vlib/logpool.h"
#include "vlib/avltree.h"
#include "vlib/util.h"
#include "vlib_private.h"

/* ************************************************************************ */
logpool_t * g_vlib_logpool = NULL;

/* format of file path when FILE* is given instead of a file path */
#define LOGPOOL_FDPATH_FMT          ";%d;%08lx;"
#define LOGPOOL_FDPATH_ARGS(file)   fileno((file)), (unsigned long)((file))

/** internal logpool flags */
typedef enum {
    LPP_NONE            = 0,
    LPP_SILENT          = 1 << 0,
    LPP_PREFIX_CASEFOLD = 1 << 1,
    LPP_DEFAULT         = LPP_NONE | LPP_PREFIX_CASEFOLD
} logpool_priv_flag_t;

/** internal logpool_file_t flags */
typedef enum {
    LFF_NONE            = 0,
    LFF_NOCLOSE         = 1 << 0,
    LFF_OPENFAILED      = 1 << 1,
} logpool_file_flags_t;

/** internal log_pool structure */
struct logpool_s {
    avltree_t *         logs;
    avltree_t *         files;
    pthread_rwlock_t    rwlock;
    unsigned int        flags;
};

/** internal file structure (data of logpool->files) */
typedef struct {
    char *          path;
    FILE *          file;
    int             use_count;
    unsigned int    flags;
} logpool_file_t;

/** internal logpool entry (data of logpool->logs) */
typedef struct {
    log_t               log;
    logpool_file_t *    file;
    int                 use_count;
} logpool_entry_t;

/* ************************************************************************ */
static logpool_entry_t *logpool_add_unlocked(
                            logpool_t *         pool,
                            log_t *             log,
                            const char *        path);

/* ************************************************************************ */
static inline int logpool_prefixcmp_internal(
                        const void * vpool1_data, const void * vpool2_data,
                        int (*cmpfun)(const char *, const char *)) {
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
    if ((log1->log.flags & LOGPOOL_FLAG_PATTERN) != (log2->log.flags & LOGPOOL_FLAG_PATTERN)) {
        return (log1->log.flags & LOGPOOL_FLAG_PATTERN) != 0 ? -1 : 1;
    }
    return cmpfun(log1->log.prefix, log2->log.prefix);
}
int logpool_prefixcmp(const void * vpool1_data, const void * vpool2_data) {
    return logpool_prefixcmp_internal(vpool1_data, vpool2_data, strcmp);
}
int logpool_prefixcasecmp(const void * vpool1_data, const void * vpool2_data) {
    return logpool_prefixcmp_internal(vpool1_data, vpool2_data, strcasecmp);
}

/* ************************************************************************ */
static inline int logpool_pathcmp_internal(
                        const void * vpool1_data, const void * vpool2_data,
                        int (*cmpfun)(const char *, const char *)) {
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
    return cmpfun(file1->path, file2->path);
}
int logpool_pathcmp(const void * vpool1_data, const void * vpool2_data) {
    return logpool_pathcmp_internal(vpool1_data, vpool2_data, strcmp);
}
int logpool_pathcasecmp(const void * vpool1_data, const void * vpool2_data) {
    return logpool_pathcmp_internal(vpool1_data, vpool2_data, strcasecmp);
}

/* ************************************************************************ */
static logpool_file_t * logpool_file_create(const char * path, FILE * file) {
    logpool_file_t * pool_file = malloc(sizeof(logpool_file_t));

    if (pool_file == NULL) {
        return NULL;
    }
    pool_file->use_count = 0;
    pool_file->flags = LFF_NONE;

    if (path != NULL && file == NULL) {
        pool_file->file = fopen(path, "a");
        if (pool_file->file == NULL) {
            LOG_WARN(g_vlib_log, "logpool: cannot open file '%s': %s", path, strerror(errno));
            pool_file->flags |= LFF_OPENFAILED; /* rfu: could be used to retry open */
        }
    } else {
        pool_file->flags |= LFF_NOCLOSE;
        pool_file->file = file;
    }
    if (pool_file->file == NULL) {
        pool_file->file = LOG_FILE_DEFAULT;
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
            int fd = fileno(pool_file->file);
            if (fd != STDERR_FILENO && fd != STDOUT_FILENO
            && (pool_file->flags & LFF_NOCLOSE) == 0) {
                struct stat stats;
                sched_yield();
                //flockfile(pool_file->file); // not necessary, we are about to close the file
                fflush(pool_file->file);
                fsync(fileno(pool_file->file));
                //funlockfile(pool_file->file); // not necessary, we are about to close the file
                fclose(pool_file->file);
                if (pool_file->path != NULL
                && stat(pool_file->path, &stats) == 0 && stats.st_size == 0) {
                    unlink(pool_file->path);
                }
            }
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
        if (logentry->log.out != NULL) {
            LOG_DEBUG(g_vlib_log, "%s(): removing log '%s'", __func__,
                      STR_CHECKNULL(logentry->log.prefix));
        }
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
    log_t       log     = { LOG_LVL_INFO, LOG_FLAG_DEFAULT | LOGPOOL_FLAG_TEMPLATE,
                            LOG_FILE_DEFAULT, NULL };
    avltree_cmpfun_t prefcmpfun;

    if (pool == NULL) {
        return NULL;
    }
    if (pthread_rwlock_init(&pool->rwlock, NULL) != 0) {
        LOG_ERROR(g_vlib_log, "error pthread_rwlock_init(): %s", strerror(errno));
        free(pool);
        return NULL;
    }
    pool->flags = LPP_DEFAULT;
    prefcmpfun = (pool->flags & LPP_PREFIX_CASEFOLD) != 0
                 ? logpool_prefixcasecmp : logpool_prefixcmp;

    if (NULL == (pool->files = avltree_create((AFL_DEFAULT | AFL_SHARED_STACK)
                                              | AFL_INSERT_NODOUBLE | AFL_REMOVE_NOFREE,
                                              logpool_pathcmp, logpool_file_free))
    ||  NULL == (pool->logs = avltree_create((AFL_DEFAULT & ~AFL_SHARED_STACK)
                                             | AFL_INSERT_IGNDOUBLE | AFL_REMOVE_NOFREE,
                                             prefcmpfun, logpool_entry_free))) {
        LOG_ERROR(g_vlib_log, "error avltree_create(logs | files) : %s", strerror(errno));
        if (pool->files != NULL) {
            avltree_free(pool->files);
        }
        pthread_rwlock_destroy(&pool->rwlock);
        free(pool);
        return NULL;
    }

    /* use the same stack for logs and files */
    pool->logs->shared = pool->files->shared;

    /* add a default log instance */
    logpool_add_unlocked(pool, &log, NULL);
    /* add the vlib log instance if it is the first logpool */
    if (g_vlib_log != NULL && g_vlib_logpool == NULL) {
        logpool_entry_t * entry;
        if (g_vlib_log->out == NULL) {
            flockfile(LOG_FILE_DEFAULT);
            g_vlib_log->out = LOG_FILE_DEFAULT;
            funlockfile(LOG_FILE_DEFAULT);
        }
        entry = logpool_add_unlocked(pool, g_vlib_log, NULL);
        log_set_vlib_instance(&(entry->log));
    }
    /* set the g_vlib_logpool */
    if (g_vlib_logpool == NULL) {
        g_vlib_logpool = pool;
    }

    return pool;
}

/* ************************************************************************ */
void                logpool_free(
                        logpool_t *         pool) {
    if (pool != NULL) {
        size_t nf;
        size_t nl;

        pthread_rwlock_wrlock(&pool->rwlock);
        if (pool == g_vlib_logpool) {
            g_vlib_logpool = NULL;
        }

        nf = avltree_count(pool->files);
        nl = avltree_count(pool->logs);
        LOG_VERBOSE(g_vlib_log, "%s(): %zu file%s, %zu log%s.", __func__,
                    nf, nf > 1 ? "s" : "",
                    nl, nl > 1 ? "s" : "");

        avltree_free(pool->logs);
        avltree_free(pool->files); /* must be last : will free rbuf stack and close files */
        pthread_rwlock_unlock(&pool->rwlock);
        pthread_rwlock_destroy(&pool->rwlock);
        memset(pool, 0, sizeof(*pool));
        free(pool);
        LOG_DEBUG(g_vlib_log, "%s(): done.", __func__);
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

    if (pool == NULL && g_vlib_logpool != NULL) {
        pool = g_vlib_logpool;
    }

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
    logpool_entry_t *entry;

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
        log.level = LOG_LVL_INFO;
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
        }

        /* Get the the Log File */
        log.out = LOG_FILE_DEFAULT;
        if (sep == '@') {
            len = strtok_ro_r((const char **) &token, ":", &next_tok, &maxlen, 0);
            LOG_DEBUG_BUF(g_vlib_log, token, len, "mod_file ");
            if (len > 0) {
                token[len] = 0;
                log.out = NULL;
                mod_file = token;
            }
        } else {
            LOG_DEBUG_BUF(g_vlib_log, token, 0, "mod_file ");
        }

        /* Get the Log flags */
        log.flags = LOG_FLAG_DEFAULT | LOGPOOL_FLAG_TEMPLATE;
        token = (char *) next_tok;
        len = maxlen;
        LOG_DEBUG_BUF(g_vlib_log, token, len, "mod_flags ");
        if (len > 0) {
            const char *    next_flag   = token;
            size_t          maxflaglen  = maxlen;
            log_flag_t      flag;
            char            nextsep = 0;

            token[len] = 0;
            log.flags = LOG_FLAG_NONE | LOGPOOL_FLAG_TEMPLATE;
            for (char sep = 0; maxflaglen > 0; sep = nextsep) {
                len = strtok_ro_r((const char **) &token, "|+-", &next_flag, &maxflaglen, 0);
                LOG_DEBUG_BUF(g_vlib_log, token, 0, "mod_flag ");
                if (len > 0) {
                    nextsep = token[len];
                    token[len] = 0;
                    if ((flag = log_flag_from_name(token)) != LOG_FLAG_UNKNOWN) {
                        if (sep == '-') {
                            log.flags &= ~flag;
                        } else {
                            static const unsigned int time_exclusive_flags
                                = LOG_FLAG_ABS_TIME | LOG_FLAG_DATETIME;
                            if ((flag & time_exclusive_flags) != 0) {
                                log.flags &= ~(time_exclusive_flags);
                            }
                            log.flags |= flag;
                        }
                    } else {
                        LOG_WARN(g_vlib_log, "warning: unknown log flag '%s'", token);
                        //FIXME: return error?
                    }
                }
            }
        }

        /* add the log to the pool */
        len = avltree_count(pool->logs);
        if ((entry = logpool_add_unlocked(pool, &log, mod_file)) == NULL) {
            LOG_ERROR(g_vlib_log, "error: logpool_add(pref:%s,lvl:%s,flg:%d,path:%s) error.",
                                  log.prefix ? log.prefix : "<null>",
                                  log_level_name(log.level),
                                  log.flags,
                                  mod_file ? mod_file : "<null>");
        }
        else {
            if (avltree_count(pool->logs) > len && entry->use_count == 1) {
                entry->use_count = 0; /* if new entry created (not replaced), set counter = 0 */
            }
            LOG_VERBOSE(g_vlib_log, "logpool_cmdline: Log ADDED "
                                    "pref:<%s> lvl:%s flags:%x out=%lx(fd %d) path:%s",
                        STR_CHECKNULL(log.prefix), log_level_name(log.level),
                        log.flags, (unsigned long) log.out,
                        log.out != NULL ? fileno(log.out) : -1, STR_CHECKNULL(mod_file));
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
    logpool_entry_t *   entry;

    if (pool == NULL || log == NULL) {
        return NULL;
    }

    pthread_rwlock_wrlock(&(pool->rwlock));

    entry = logpool_add_unlocked(pool, log, path);

    pthread_rwlock_unlock(&(pool->rwlock));

    return entry == NULL ? NULL : &(entry->log);
}

/* ************************************************************************ */
static logpool_entry_t *logpool_add_unlocked(
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
    logentry->log = *log;
    logentry->log.flags &= ~(LOG_FLAG_CLOSEFILE | LOG_FLAG_FREELOG);
    logentry->log.flags |= LOG_FLAG_FREEPREFIX;
    if (log->prefix != NULL) {
        logentry->log.prefix = strdup(log->prefix);
    }
    if ((pool->flags & LPP_SILENT) != 0)
        logentry->log.flags |= LOG_FLAG_SILENT;
    if (fnmatch_patternidx(log->prefix) >= 0) {
        logentry->log.flags |= LOGPOOL_FLAG_PATTERN;
    } else {
        logentry->log.flags &= ~(LOGPOOL_FLAG_PATTERN);
    }
    logentry->file = NULL;
    logentry->use_count = 1;

    /* if path not given, use ';fd;fileptr;' as path, else get absolute path from given path. */
    if (path == NULL) {
        if (logentry->log.out == NULL)
            logentry->log.out = LOG_FILE_DEFAULT;
        snprintf(abspath, sizeof(abspath), LOGPOOL_FDPATH_FMT,
                LOGPOOL_FDPATH_ARGS(logentry->log.out));
    } else {
        logentry->log.out = NULL;
        if (*path == ';') {
            str0cpy(abspath, path, sizeof(abspath));
            path = NULL;
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
        /* a new file has to be created (not found in pool->files) */
        pfile = logpool_file_create(tmpfile.path, tmpfile.file);
        if (pfile == NULL || avltree_insert(pool->files, pfile) == NULL) {
            LOG_DEBUG(g_vlib_log, "%s(): new file for new logentry creation error", __func__);
            if (preventry == logentry /* the new logentry must be deleted */
            && avltree_remove(pool->logs, logentry) != NULL) {
                logpool_entry_free(logentry);
            } /* otherwise we let the preventry unchanged */
            /* need to delete new created file, it is not used and not inserted in pool->files */
            logpool_file_free(pfile);
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

        LOG_DEBUG(g_vlib_log, "LOGENTRY <%s> REPLACED by <%s> (%s%s)",
                  STR_CHECKNULL(preventry->log.prefix), STR_CHECKNULL(logentry->log.prefix),
                  preventry->file == pfile ? "same file:" : "", STR_CHECKNULL(pfile->path));

        /* lock log file before continuing, to not disturb threads owning this log entry */
        logout = preventry->log.out == NULL ? LOG_FILE_DEFAULT : preventry->log.out;
        flockfile(logout);

        if (preventry->file != pfile) { /* pointer comparison OK as doubles are forbiden. */
            /* the new logentry has a different file */
            LOG_DEBUG(g_vlib_log, "LOGENTRY %s REPLACED: FILE <%s> replaced by <%s>",
                      STR_CHECKNULL(preventry->log.prefix), STR_CHECKNULL(preventry->file->path),
                      STR_CHECKNULL(pfile->path));
            if (--preventry->file->use_count == 0) {
                LOG_DEBUG(g_vlib_log, "LOGENTRY %s REPLACED: previous file <%s> is NO LONGER USED",
                          STR_CHECKNULL(preventry->log.prefix), STR_CHECKNULL(preventry->file->path));
                /* the file of previous logentry is no longer used -> remove it. */
                if (avltree_remove(pool->files, preventry->file) == NULL) {
                    LOG_ERROR(g_vlib_log, "error: cannot remove file <%s> from logpool",
                              STR_CHECKNULL(preventry->file->path));
                } else {
                    file_to_free = preventry->file;
                }
            }
        } else {
            /* new log has the same file as previous: decrement because ++use_count below. */
            --preventry->file->use_count;
        }
        /* prefix of prev entry is kept, new one (equal) is freed */
        if (logentry->log.prefix != NULL) {
            free(logentry->log.prefix);
            logentry->log.prefix = preventry->log.prefix;
        }
        /* template flag of previous logentry is kept */
        logentry->log.flags |= (preventry->log.flags & LOGPOOL_FLAG_TEMPLATE);
        /* copy new log entry data into previous entry (data changes, log pointer does not */
        logentry->use_count = preventry->use_count;
        *preventry = *logentry;
        logentry->log.prefix = NULL; /* logentry->log.prefix already freed */
        logentry->log.out = NULL;
        logpool_entry_free(logentry);

        /* we can now unlock the old file, and free it as updated log entry is ready to be used */
        funlockfile(logout);
        if (file_to_free != NULL) {
            logpool_file_free(file_to_free);
        }
    }

    /* finish initialization and return log instance */
    ++pfile->use_count;

    return preventry;
}

/* ************************************************************************ */
static inline int   logpool_remove_unlocked(
                        logpool_t *         pool,
                        logpool_entry_t *   search) {
    logpool_entry_t *   logentry;

    if ((logentry = avltree_remove(pool->logs, search)) != NULL) {
        if (logentry->file != NULL && --logentry->file->use_count == 0
        && avltree_remove(pool->files, logentry->file) != NULL) {
            logpool_file_free(logentry->file);
        }
        logpool_entry_free(logentry);
    }

    return logentry == NULL ? -1 : 0;
}

/* ************************************************************************ */
int                 logpool_remove(
                        logpool_t *         pool,
                        log_t *             log) {
    int             ret;
    logpool_entry_t search;

    if (pool == NULL || log == NULL) {
        return -1;
    }

    search.log.prefix = log->prefix;
    search.log.flags = log->flags;
    if (fnmatch_patternidx(search.log.prefix) >= 0) {
        search.log.flags |= LOGPOOL_FLAG_PATTERN;
    } else {
        search.log.flags &= ~(LOGPOOL_FLAG_PATTERN);
    }

    pthread_rwlock_wrlock(&pool->rwlock);
    ret = logpool_remove_unlocked(pool, &search);
    pthread_rwlock_unlock(&pool->rwlock);

    return ret;
}

/* ************************************************************************ */
int                 logpool_release(
                        logpool_t *         pool,
                        log_t *             log) {
    int                 ret = -1;
    logpool_entry_t *   entry;
    logpool_entry_t     search;

    if (pool == NULL || log == NULL) {
        errno = EFAULT;
        return ret;
    }

    search.log.prefix = log->prefix;
    search.log.flags = log->flags;

    if (fnmatch_patternidx(search.log.prefix) >= 0) {
        search.log.flags |= LOGPOOL_FLAG_PATTERN;
    } else {
        search.log.flags &= ~(LOGPOOL_FLAG_PATTERN);
    }

    pthread_rwlock_wrlock(&pool->rwlock);

    if ((entry = avltree_find(pool->logs, &search)) != NULL) {
        if (entry->use_count > 0 && --(entry->use_count) > 0) {
            LOG_DEBUG(g_vlib_log, "LOGPOOL entry %s NOT released (use_count %d).",
                      STR_CHECKNULL(entry->log.prefix), entry->use_count);
            errno = EBUSY;
            ret = entry->use_count;
        } else if ((entry->log.flags & LOGPOOL_FLAG_TEMPLATE) != 0) {
            errno = EACCES;
            LOG_DEBUG(g_vlib_log, "LOGPOOL entry '%s' NOT released (template).",
                      STR_CHECKNULL(entry->log.prefix));
        } else {
            LOG_DEBUG(g_vlib_log, "LOGPOOL entry '%s' will be released.",
                      STR_CHECKNULL(search.log.prefix));
            ret = logpool_remove_unlocked(pool, entry);
        }
    } else {
        LOG_DEBUG(g_vlib_log, "warning: LOGPOOL entry '%s' not found",
                  STR_CHECKNULL(search.log.prefix));
    }

    pthread_rwlock_unlock(&pool->rwlock);

    return ret;
}

/* ************************************************************************ */
log_t *             logpool_find(
                        logpool_t *         pool,
                        const char *        prefix) {
    log_t *             result = NULL;
    logpool_entry_t *   entry;
    logpool_entry_t     ref;

    if (pool == NULL) {
        return NULL;
    }

    pthread_rwlock_rdlock(&pool->rwlock);

    ref.log.prefix = (char *) prefix;
    ref.log.flags = LOG_FLAG_NONE;

    if (fnmatch_patternidx(ref.log.prefix) >= 0) {
        ref.log.flags |= LOGPOOL_FLAG_PATTERN;
    } else {
        ref.log.flags &= ~(LOGPOOL_FLAG_PATTERN);
    }

    if ((entry = avltree_find(pool->logs, &ref)) != NULL) {
        result = &(entry->log);
    }

    pthread_rwlock_unlock(&pool->rwlock);

    return result;
}

/* ************************************************************************ */
typedef struct {
    char *              prefix;
    logpool_entry_t *   result;
    int                 fnm_flag;
    int                 bfirst_is_pattern;
} logpool_findpattern_t;
static avltree_visit_status_t   logpool_findpattern_visit(
                                    avltree_t *                         tree,
                                    avltree_node_t *                    node,
                                    const avltree_visit_context_t *     context,
                                    void *                              user_data) {
    logpool_entry_t *       entry = (logpool_entry_t *) node->data;
    logpool_findpattern_t * data = (logpool_findpattern_t *) user_data;
    (void) tree;
    (void) context;

    if (entry->log.prefix == NULL) {
        /* should not happen */
        return AVS_ERROR;
    }
    if (fnmatch(entry->log.prefix, data->prefix, data->fnm_flag) == 0) {
        data->result = entry;
        return AVS_FINISHED;
    }
    if (data->bfirst_is_pattern && *(entry->log.prefix) != '*'
    &&  *(entry->log.prefix) != '[' &&  *(entry->log.prefix) != '?') {
       return AVS_FINISHED;
    }
    return AVS_CONTINUE;
}
static logpool_entry_t * logpool_findpattern(
                            logpool_t *         pool,
                            logpool_entry_t *   search) {

    logpool_entry_t         min, max;
    char                    minpref[2], maxpref[3];
    logpool_findpattern_t   data = {
        .result = NULL, .prefix = search->log.prefix, .bfirst_is_pattern = 0,
        .fnm_flag = (pool->flags & LPP_PREFIX_CASEFOLD) != 0 ? FNM_CASEFOLD : 0 };

    /* logpool_prefixcmp ensures that patterns are always smaller than regular strings
     * then it is possible to fastly scan the patterns in the tree without visiting
     * non-pattern strings and scan regular strings without visiting patterns.
     * - first pass on patterns starting with first char of prefix to be found
     * - second pass on patterns starting with a pattern */
    minpref[0] = search->log.prefix[0];
    minpref[1] = 0;
    maxpref[0] = search->log.prefix[0];
    maxpref[1] = 0x7f;
    maxpref[2] = 0;
    min.log.flags = LOGPOOL_FLAG_PATTERN;
    min.log.prefix = minpref;
    max.log.flags = LOGPOOL_FLAG_PATTERN;
    max.log.prefix = maxpref;

    LOG_DEBUG(g_vlib_log, "%s(): looking for patterns matching '%s' (2 passes)", __func__,
              STR_CHECKNULL(search->log.prefix));

    for (int i = 0; i < 2; ++i) {
        if (avltree_visit_range(pool->logs, &min, &max,
                            logpool_findpattern_visit, &data, AVH_INFIX) == AVS_FINISHED
        &&  data.result != NULL) {
            return data.result;
        }
        /* empty patterns for second loop */
        minpref[0] = 0;
        maxpref[0] = 0x7f;
        maxpref[1] = 0;
        data.bfirst_is_pattern = 1;
    }
    return NULL;
}
/* ************************************************************************ */
log_t *             logpool_getlog(
                        logpool_t *         pool,
                        const char *        prefix,
                        int                 flags) {
    logpool_entry_t *   entry;
    logpool_entry_t     ref;

    if (pool == NULL) {
        return NULL;
    }
    ref.log.prefix = (char *) prefix;
    ref.log.flags = LOG_FLAG_NONE;

    //TODO: think about optimization here to acquire only a read lock in some cases
    /* must always acquire wrlock because of ++(entry->use_count below
    if ((flags & LPG_TRUEPREFIX) == 0) {
        pthread_rwlock_rdlock(&pool->rwlock);
    } else {
        pthread_rwlock_wrlock(&pool->rwlock);
    } */
    pthread_rwlock_wrlock(&pool->rwlock);

    /* look for the requested log instance */
    if ((entry = avltree_find(pool->logs, &ref)) == NULL) {
        /* look for a log entry pattern matching the requested log */
        if ((flags & LPG_NO_PATTERN) != 0
        ||  (entry = logpool_findpattern(pool, &ref)) == NULL) {
            if ((flags & LPG_NODEFAULT) != 0) {
                /* don't use default if flag forbids it */
                pthread_rwlock_unlock(&pool->rwlock);
                return NULL;
            }
            /* look for a default log instance */
            ref.log.prefix = NULL;
            entry = avltree_find(pool->logs, &ref);
        }
    }

    if (entry != NULL && (flags & LPG_TRUEPREFIX) != 0
    &&  entry->log.prefix != prefix
    &&  (entry->log.prefix == NULL || prefix == NULL || strcmp(entry->log.prefix, prefix))) {
        /* duplicate log and put requested prefix */
        memcpy(&(ref.log), &(entry->log), sizeof(ref.log));
        ref.log.prefix = (char *) prefix;
        ref.log.flags &= ~(LOGPOOL_FLAG_TEMPLATE);
        entry = logpool_add_unlocked(pool, &(ref.log), entry->file->path);
        LOG_DEBUG(g_vlib_log, "LOGPOOL: created new entry '%s'",
                  STR_CHECKNULL(entry->log.prefix));
    } else if (entry != NULL) {
        ++(entry->use_count);
    }
    pthread_rwlock_unlock(&pool->rwlock);

    return entry == NULL ? NULL : &(entry->log);
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
                STR_CHECKNULL(entry->log.prefix),
                (unsigned long) entry->log.out,
                entry->log.out ? fileno(entry->log.out) : -1,
                (unsigned long) (entry->file ? entry->file->file : NULL),
                entry->file && entry->file->file ? fileno(entry->file->file) : -1,
                entry->file ? entry->file->use_count : -1,
                entry->file && entry->file->path ? entry->file->path : STR_NULL);
    } else {
        LOG_INFO(log, "LOGPOOL: ENTRY " STR_NULL);
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
                 STR_CHECKNULL(file->path));
    } else {
        LOG_INFO(log, "LOGPOOL: FILE " STR_NULL);
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

    if ((entry->log.flags & LOGPOOL_FLAG_PATTERN) == 0)
        len = snprintf(name, name_sz, "%s", STR_CHECKNULL(entry->log.prefix));
    else
        len = snprintf(name, name_sz, "/%s/", STR_CHECKNULL(entry->log.prefix));
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
static void        logpool_logpath_freeone(void * vdata) {
    logpool_logpath_t * logpath = (logpool_logpath_t *) vdata;

    if (logpath->path != NULL) {
        free(logpath->path);
    }
    if (logpath->log != NULL) {
        log_destroy(logpath->log);
        if ((logpath->log->flags & LOG_FLAG_FREELOG) == 0)
            free(logpath->log);
    }
    free(logpath);
}
/* ************************************************************************ */
int                 logpool_logpath_free(
                        logpool_t *         pool,
                        slist_t *           list) {
    (void) pool;
    slist_free(list, logpool_logpath_freeone);
    return 0;
}

typedef struct {
    slist_t *       list;
    char            path[PATH_MAX*2];
} logpool_findbypath_visit_t;
static avltree_visit_status_t logpool_findbypath_visit(
                                    avltree_t *                         tree,
                                    avltree_node_t *                    node,
                                    const avltree_visit_context_t *     context,
                                    void *                              user_data) {
    logpool_findbypath_visit_t *    data = (logpool_findbypath_visit_t *) user_data;
    logpool_entry_t *               logentry = (logpool_entry_t *) node->data;
    int found = 0;
    (void) tree;
    (void) context;

    if (logentry == NULL) {
        return AVS_ERROR;
    }
    if (*(data->path) == 0) {
        int fd = fileno(logentry->file->file);
        found = (fd == STDOUT_FILENO || fd == STDERR_FILENO);
    } else {
        found = (fnmatch(data->path, logentry->file->path, 0) == 0);
    }
    if (found) {
        logpool_logpath_t * logpath = malloc(sizeof(logpool_logpath_t));
        if (logpath != NULL) {
            logpath->log = log_create(&(logentry->log));
            if ((logentry->file->flags & LFF_NOCLOSE) != 0) {
                logpath->path = NULL;
            } else {
                logpath->path = strdup(data->path);
            }
            data->list = slist_prepend(data->list, logpath);
        }
    }

    return AVS_CONTINUE;
}
static slist_t *    logpool_findbypath_unlocked(
                        logpool_t *         pool,
                        const char *        path) {
    logpool_findbypath_visit_t data;

    if (vabspath(data.path, sizeof(data.path) / sizeof(*(data.path)), path, NULL) <= 0) {
        *(data.path) = 0;
    }
    data.list = NULL;

    if (avltree_visit(pool->logs, logpool_findbypath_visit, &data, AVH_PREFIX)
            != AVS_FINISHED) {
        logpool_logpath_free(pool, data.list);
        return NULL;
    }

    return data.list;
}
/* ************************************************************************ */
slist_t *           logpool_findbypath(
                        logpool_t *         pool,
                        const char *        path) {
    slist_t * list;

    if (pool == NULL) {
        return NULL;
    }

    pthread_rwlock_rdlock(&pool->rwlock);
    list = logpool_findbypath_unlocked(pool, path);
    pthread_rwlock_unlock(&pool->rwlock);

    return list;
}
/* ************************************************************************ */
int logpool_replacefile(
                        logpool_t *         pool,
                        slist_t *           logs,
                        const char *        newpath,
                        slist_t **          pbackup) {
    slist_t *   logs_tofree = NULL;
    char        abspath[PATH_MAX*2];
    int         nerrors = 0;

    if (pool == NULL
    || (newpath != NULL && vabspath(abspath, sizeof(abspath) / sizeof(*abspath),
                                    newpath, NULL) <= 0)
    || (pbackup != NULL && logs != NULL && *pbackup == logs)) {
        return -1;
    }
    if (pbackup != NULL) {
        *pbackup = NULL;
    }

    /* LOG_VERBOSE(g_vlib_log, "%s(): about to replace logs with '%s'",
                __func__, STR_CHECKNULL(newpath)); */

    pthread_rwlock_wrlock(&pool->rwlock);

    /* create internal list of logs using stdout/stderr, if logs not given (NULL) */
    if (logs == NULL) {
        logs = logs_tofree = logpool_findbypath_unlocked(pool, NULL);
        if (logs == NULL)
            ++nerrors;
    }

    /* for each log matching, backup it if needed, and assign the new path */
    SLIST_FOREACH_DATA(logs, logpath, logpool_logpath_t *) {
        log_t               newlog = *(logpath->log);
        logpool_entry_t *   preventry;
        char *              tmppath = newpath != NULL ? abspath : logpath->path;

        if (tmppath != NULL) {
            FILE * tmpfile = fopen(tmppath, "a");
            if (tmpfile == NULL) {
                ++nerrors;
            } else {
                fclose(tmpfile);
            }
        }
        if (pbackup != NULL) {
            logpool_entry_t entry;
            memcpy(&(entry.log), logpath->log, sizeof(*(logpath->log)));
            if ((preventry = avltree_find(pool->logs, &entry)) != NULL) {
                logpool_logpath_t * logpath_bak = malloc(sizeof(logpool_logpath_t));

                if (logpath_bak != NULL) {
                    logpath_bak->log = log_create(&(preventry->log));
                    if ((preventry->file->flags & LFF_NOCLOSE) != 0) {
                        logpath_bak->path = NULL;
                    } else {
                        logpath_bak->path = strdup(preventry->file->path);
                    }
                    *pbackup = slist_prepend(*pbackup, logpath_bak);
                } else ++nerrors;
            } else ++nerrors;
        }
        if (tmppath != NULL) {
            newlog.out = NULL;
        }
        if (logpool_add_unlocked(pool, &newlog, tmppath) == NULL) {
            ++nerrors;
        }
    }

    pthread_rwlock_unlock(&pool->rwlock);

    if (logs_tofree != NULL) {
        logpool_logpath_free(pool, logs_tofree);
    }

    return nerrors;
}

/* ************************************************************************ */

