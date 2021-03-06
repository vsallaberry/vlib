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
 * Simple avl tree utilities.
 * AVL: 1962, Georgii Adelson-Velsky & Evguenii Landis.
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "vlib/avltree.h"
#include "vlib/log.h"
#include "vlib/rbuf.h"
#include "vlib/job.h"
#include "vlib/term.h"

#include "vlib_private.h"

/*****************************************************************************/
#define AVLTREE_STACK_SZ    32 /* 10^5 elts:depth=20, 10^6:24, 10^7:28 */

/** visit data used for deletion and insertion visitors */
typedef struct {
    void *              newdata;        /* IN  : value of new node */
    avltree_node_t *    newnode;        /* OUT : pointer to new created node */
    char                new_balance;    /* OUT : new balance of inserted node */
    avltree_node_t **   prev_nodep;     /* internal: previous node (prefix:parent, suffix:child) */
} avltree_visit_insert_t;

/** visit data used for range visitor */
typedef struct {
    void *              min;
    void *              max;
    avltree_visitfun_t  visit;
    void *              user_data;
} avltree_visit_range_t;

/*****************************************************************************/
static avltree_node_t * avltree_rotate_left(avltree_t * tree, avltree_node_t * node, int update);
static avltree_node_t * avltree_rotate_right(avltree_t * tree, avltree_node_t * node, int update);
static avltree_node_t * avltree_rotate_rightleft(avltree_t * tree, avltree_node_t * node);
static avltree_node_t * avltree_rotate_leftright(avltree_t * tree, avltree_node_t * node);
static int              avltree_visit_rebalance(
                            avltree_t *                     tree,
                            avltree_node_t *                node,
                            const avltree_visit_context_t * context,
                            void *                          user_data);
static int              avltree_visit_insert(
                            avltree_t *                     tree,
                            avltree_node_t *                node,
                            const avltree_visit_context_t * context,
                            void *                          user_data);
static int              avltree_visit_remove(
                            avltree_t *                     tree,
                            avltree_node_t *                node,
                            const avltree_visit_context_t * context,
                            void *                          user_data);
static int              avltree_visit_free(
                            avltree_t *                     tree,
                            avltree_node_t *                node,
                            const avltree_visit_context_t * context,
                            void *                          user_data);
static int              avltree_visit_find_range(
                            avltree_t *                     tree,
                            avltree_node_t *                node,
                            const avltree_visit_context_t * context,
                            void *                          user_data);

/*****************************************************************************/
static inline avltree_node_t * avltree_node_alloc(avltree_t * tree) {
    (void)tree;
    return malloc(sizeof(avltree_node_t));
}

/*****************************************************************************/
static inline void avltree_node_free(
                        avltree_t * tree, avltree_node_t * node, int freedata) {
    if (tree && node) {
        if (freedata && tree->free) {
            tree->free(node->data);
        }
        free(node);
    }
}

/*****************************************************************************/
avltree_t *         avltree_create(
                        avltree_flags_t             flags,
                        avltree_cmpfun_t            cmpfun,
                        avltree_freefun_t           freefun) {
    avltree_t * tree;

    if (cmpfun == NULL) {
        return NULL; /* avl tree is based on node comparisons. */
    }
    tree = malloc(sizeof(avltree_t));
    if (tree == NULL) {
        return NULL;
    }
    /* create a shared stack if requested in flags */
    if ((flags & AFL_SHARED_STACK) != 0) {
        if ((tree->shared = malloc(sizeof(*(tree->shared)))) == NULL) {
            free(tree);
            return NULL;
        }
        tree->shared->in_use = 0;
        tree->shared->context = NULL;
        if (NULL == (tree->shared->stack
                    = rbuf_create(AVLTREE_STACK_SZ, RBF_DEFAULT | RBF_SHRINK_ON_RESET))
        || NULL == (tree->shared->context = malloc(sizeof(*(tree->shared->context))))
        || pthread_mutex_init(&(tree->shared->mutex), NULL) != 0) {
            rbuf_free(tree->shared->stack);
            if (tree->shared->context != NULL)
                free(tree->shared->context);
            free(tree->shared);
            free(tree);
            return NULL;
        }
   } else {
        tree->shared = NULL;
    }
    tree->root = NULL;
    tree->n_elements = 0;
    tree->flags = flags;
    tree->cmp = cmpfun;
    tree->free = freefun;
    return tree;
}

/*****************************************************************************/
avltree_node_t *    avltree_node_create(
                        avltree_t *                 tree,
                        void *                      data,
                        avltree_node_t *            left,
                        avltree_node_t *            right) {
    avltree_node_t * new = avltree_node_alloc(tree);

    if (new == NULL)
        return NULL;
    new->data = data;
    new->right = right;
    new->left = left;
    new->balance = 0;
    return new;
}

/****************************************************************************
 * avltree_insert(): for now, the avltree_visit(AVH_PREFIX|AVH_SUFFIX) is
 * used. lets see later for optimization, but at the moment, this allows
 * debugging and optimizing avltree_visit */
void *              avltree_insert(
                        avltree_t *                 tree,
                        void *                      data) {
    avltree_visit_insert_t    insert_data;

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (tree->root == NULL) {
        /* particular case when root is NULL : allocate it here without visiting */
        if ((tree->root = avltree_node_create(tree, data, NULL, NULL)) == NULL) {
            if (errno == 0) {
                errno = ENOMEM;
            }
            return NULL;
        }
        ++tree->n_elements;
        LOG_DEBUG(g_vlib_log, "created root 0x%lx data 0x%ld left:0x%lx right:0x%lx",
                  (unsigned long)tree->root, (long)tree->root->data,
                  (unsigned long)tree->root->left, (unsigned long)tree->root->right);
        if (data == NULL) {
            errno = 0;
        }
        return data;
    }
    insert_data.newdata = data;
    insert_data.newnode = NULL;
    insert_data.prev_nodep = &(tree->root);
    insert_data.new_balance = 0;
    if (avltree_visit(tree, avltree_visit_insert, &insert_data, AVH_PREFIX | AVH_SUFFIX)
            == AVS_FINISHED) {
        if (insert_data.new_balance != 0) {
            /* new_balance is 0 when node has been ignored or replaced
             * (AFL_INSERT_IGNDOUBLE, AFL_INSERT_REPLACE) */
            ++tree->n_elements;
        }
        if (insert_data.newdata == NULL) {
            errno = 0;
        }
        return insert_data.newdata;
    }
    if (errno == 0) {
        errno = EAGAIN;
    }
    return NULL;
}

/*****************************************************************************/
int                avltree_clear(
                        avltree_t *                 tree) {
    avltree_visit_status_t  ret;
    unsigned int            how = AVH_PREFIX;

    if (tree == NULL) {
        return AVS_ERROR;
    }
    if ((tree->flags & AFL_FREE_PARALLEL) != 0) {
        how |= AVH_PARALLEL;
    }
    if ((ret = avltree_visit(tree, avltree_visit_free, NULL, how)) != AVS_FINISHED) {
        LOG_WARN(g_vlib_log, "warning avltree_visit_free() error");
    }
    tree->root = NULL;
    tree->n_elements = 0;

    return ret;
}

/*****************************************************************************/
void                avltree_free(
                        avltree_t *                 tree) {
    unsigned int how = AVH_PREFIX;

    if (tree == NULL) {
        return ;
    }
    if ((tree->flags & AFL_FREE_PARALLEL) != 0) {
        how |= AVH_PARALLEL;
    }
    if (avltree_visit(tree, avltree_visit_free, NULL, how) != AVS_FINISHED) {
        LOG_WARN(g_vlib_log, "warning avltree_visit_free() error");
    }
    if (tree->shared != NULL && (tree->flags & AFL_SHARED_STACK) != 0) {
        if (tree->shared->in_use != 0)
            LOG_WARN(g_vlib_log, "warning tree shared resources in use !");
        tree->shared->in_use = 1;
        rbuf_free(tree->shared->stack);
        if (tree->shared->context != NULL)
            free(tree->shared->context);
        pthread_mutex_destroy(&(tree->shared->mutex));
        free(tree->shared);
    }
    free(tree);
}

/** avltree_find_next() context type */
typedef avltree_node_t * avltree_find_context_t;
#define AVLTREE_FIND_CTX_INITIALIZER(_tree)     ((_tree)->root)
#define AVLTREE_DECLARE_FIND_CTX(_tree, _name)  \
            avltree_find_context_t _name = AVLTREE_FIND_CTX_INITIALIZER(_tree)

#if 0 // Not published yet: need testing
/* ### FOR avltree.h ### */
/** avltree_find(). Please use avltree_find_next() if you need to find twins. */
/** avltree_find_next()
 * complexity: O(log2(n))
 * @param find_context, the pointer to find context which must be decalred or
 *        initialized with: 'AVLTEE_DECLARE_FIND_CTX(tree, <var_name>);'
 *        or '<varname> = AVLTREE_FIND_CTX_INITIALIZER(tree);'. No Free needed.
 * @return the next matching element
 *         (errno is only set to 0 if element is NULL),
 *         or NULL on error with errno set. */
void *              avltree_find_next(
                        avltree_t *                 tree,
                        const void *                data,
                        avltree_find_context_t *    find_context);
#endif

/*****************************************************************************/
static inline void * avltree_find_next_internal(
                        avltree_t *                 tree,
                        const void *                data,
                        avltree_find_context_t *    find_context) {

    avltree_node_t *    node;
    int                 cmp;

    node = *find_context;
    while (node != NULL) {
        cmp = tree->cmp(data, node->data);
        if (cmp == 0) {
            if (node->data == NULL) {
                errno = 0;
            }
            *find_context = node->left; /* next one if any is on left child */
            return node->data;
        } else if (cmp < 0) {
            node = node->left;
        } else {
            node = node->right;
        }
    }
    errno = ENOENT;
    return NULL;
}

#if 0 /* not published yet, need testing */
/*****************************************************************************/
void *              avltree_find_next(
                        avltree_t *                 tree,
                        const void *                data,
                        avltree_find_context_t *    find_context) {
    if (tree == NULL || find_context == NULL) {
        errno = EINVAL;
        return NULL;
    }
    return avltree_find_next_internal(tree, data, find_context);
}
#endif

/*****************************************************************************/
void *              avltree_find(
                        avltree_t *                 tree,
                        const void *                data) {
    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    } else {
        AVLTREE_DECLARE_FIND_CTX(tree, find_context);
        return avltree_find_next_internal(tree, data, &find_context);
    }
}

/*****************************************************************************/
void *              avltree_find_min(
                        avltree_t *                 tree) {
    avltree_node_t *    node;

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (tree->root == NULL) {
        errno = ENOENT;
        return NULL;
    }
    node = tree->root;
    while (node->left != NULL) {
        node = node->left;
    }
    if (node->data == NULL) {
        errno = 0;
    }
    return node->data;
}

/*****************************************************************************/
void *              avltree_find_max(
                        avltree_t *                 tree) {
    avltree_node_t *    node;

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (tree->root == NULL) {
        errno = ENOENT;
        return NULL;
    }
    node = tree->root;
    while (node->right != NULL) {
        node = node->right;
    }
    if (node->data == NULL) {
        errno = 0;
    }
    return node->data;
}

/*****************************************************************************/
unsigned int        avltree_find_depth(
                        avltree_t *                 tree) {
    avltree_node_t *    node;
    unsigned int        depth = 0;

    if (tree == NULL) {
        errno = EINVAL;
        return 0;
    }
    node = tree->root;
    while (node != NULL) {
        ++depth;
        if (node->balance > 0) {
            node = node->right;
        } else {
            node = node->left;
        }
    }
    if (depth == 0) {
        errno = 0;
    }
    return depth;
}

/*****************************************************************************/
size_t              avltree_count(
                        avltree_t *                 tree) {
    if (tree == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (tree->n_elements == 0) {
        errno = 0;
    }
    return tree->n_elements;
}
/*****************************************************************************/
size_t              avltree_memorysize(
                        avltree_t *                 tree) {
    size_t size;
    if (tree == NULL) {
        errno = EINVAL;
        return 0;
    }
    size = sizeof(avltree_t);
    if (tree->shared != NULL && (tree->flags & AFL_SHARED_STACK) != 0) {
        size += sizeof(*(tree->shared));
        if (tree->shared->stack != NULL) {
            size += rbuf_memorysize(tree->shared->stack);
        }
        if (tree->shared->context != NULL) {
            size += sizeof(*(tree->shared->context));
        }
    }
    /* return value cannot be 0, no need to set errno */
    return size + (avltree_count(tree) * sizeof(avltree_node_t));
}

/*****************************************************************************/
#define AGC_FIRST   1
#define AGC_SECOND  2
static inline avltree_node_t *  avltree_visit_get_child(
                                    int                         what,
                                    avltree_visit_status_t      ret,
                                    avltree_visit_how_t         how,
                                    avltree_node_t *            left,
                                    avltree_node_t *            right) {
    /* invert [ret, left and right] if the visit is from right to left */
    if (((how & (AVH_BREADTH | AVH_RIGHT)) == AVH_BREADTH)
    ||  ((how & (AVH_RIGHT | AVH_BREADTH)) == AVH_RIGHT)) {
        avltree_node_t * tmp = left;
        left = right;
        right = tmp;
        if ((ret & (AVS_GO_LEFT | AVS_GO_RIGHT)) == AVS_GO_LEFT) {
           ret = AVS_GO_RIGHT;
        } else if ((ret & (AVS_GO_LEFT | AVS_GO_RIGHT)) == AVS_GO_RIGHT) {
           ret = AVS_GO_LEFT;
        }
    }
    if (what == AGC_FIRST) {
        if (left != NULL && (ret == AVS_GO_LEFT || ret == AVS_CONTINUE)) {
            return left;
        }
    } else if (right != NULL && (ret == AVS_GO_RIGHT || ret == AVS_CONTINUE)) {
        return right;
    }
    return NULL;
}

/*****************************************************************************/
static inline int avltree_visit_resources_get(
                        avltree_t *                 tree,
                        rbuf_t **                   pstack,
                        avltree_visit_context_t **  pcontext) {
    if (tree->shared != NULL) {
        pthread_mutex_lock(&(tree->shared->mutex));
        if (tree->shared->in_use == 0
        && tree->shared->stack != NULL && tree->shared->context != NULL) {
            *pstack = tree->shared->stack;
            *pcontext = tree->shared->context;
            tree->shared->in_use = 1;
            pthread_mutex_unlock(&(tree->shared->mutex));
            return AVS_FINISHED;
        }
        pthread_mutex_unlock(&(tree->shared->mutex));
    }
    if ((*pcontext = malloc(sizeof(**pcontext))) == NULL) {
        return AVS_ERROR;
    }
    if ((*pstack = rbuf_create(AVLTREE_STACK_SZ, RBF_DEFAULT)) == NULL) {
        if (*pcontext != NULL)
            free(*pcontext);
        return AVS_ERROR;
    }
    return AVS_FINISHED;
}

/*****************************************************************************/
static inline void avltree_visit_resources_free(
                        avltree_t *                 tree,
                        rbuf_t *                    stack,
                        avltree_visit_context_t *   context) {

    if (tree->shared != NULL
    &&  stack != NULL && stack == tree->shared->stack) {
        rbuf_reset(stack);
        pthread_mutex_lock(&(tree->shared->mutex));
        tree->shared->in_use = 0;
        pthread_mutex_unlock(&(tree->shared->mutex));
        return ;
    }
    if (context != NULL)
        free(context);
    rbuf_free(stack);
}

/*****************************************************************************/
typedef struct {
    avltree_visitfun_t  visit;
    void *              user_data;
    int                 how;
} avltree_visit_parallel_common_t;
typedef struct {
    avltree_t                           tree;
    vjob_t *                            job;
    avltree_node_t *                    parent; /* this not the parent of tree.root */
    avltree_visit_parallel_common_t *   common;
    void *                              job_user_data; /* !!! MUST BE LAST !!! */
} avltree_visit_parallel_t;

static void *       avltree_visit_job(void * vdata) {
    avltree_visit_parallel_t * data = (avltree_visit_parallel_t *) vdata;

    return (void *)((long) avltree_visit(&(data->tree), data->common->visit,
                                         data->job_user_data, data->common->how));
}

/*****************************************************************************/
#define AVLTREE_JOB_BYIDX(_data, _idx, _sz) \
    ((avltree_visit_parallel_t *) (((unsigned char *) (_data)) + ((_idx) * (_sz))))

/*****************************************************************************/
static int          avltree_visit_parallel(
                        avltree_t *                 tree,
                        avltree_visitfun_t          visit,
                        void *                      user_data,
                        avltree_visit_how_t         how) {

    avltree_visit_parallel_common_t common_data;
    unsigned char *             data;
    unsigned int                ncpus;
    unsigned int                njobs, depth, maxjobs, nparents;
    unsigned int                userdatasz, structsz;
    avltree_node_t *            node;
    int                         ret = AVS_FINISHED;
    rbuf_t *                    fifo;
    avltree_visit_context_t *   ctx;

    /* alloc data */
    userdatasz = user_data != NULL ? AVH_PARALLEL_DATASZ(how) : 0U;
    structsz = sizeof(avltree_visit_parallel_t) + userdatasz;

    if ((ncpus = vjob_cpu_nb()) < 2U)
        ncpus = 2U;
    maxjobs = (ncpus / 2U) * 4U;
    if ((data = malloc(maxjobs * (structsz))) == NULL) {
        return AVS_ERROR;
    }
    if (avltree_visit_resources_get(tree, &fifo, &ctx) != AVS_FINISHED) {
        free(data);
        return AVS_ERROR;
    }

    LOG_DEBUG(g_vlib_log, "%s(): %zu nodes, %u CPUs, how:%d, datasz=%u", __func__,
              tree->n_elements, ncpus, how, userdatasz);

    /* init threads visit data */
    common_data.user_data = user_data;
    common_data.visit = visit;
    common_data.how = how | AVH_PARALLEL; /* so as threads know they are in PARALELL mode */
    for (njobs = 0; njobs < maxjobs; ++njobs) {
        avltree_visit_parallel_t * job = AVLTREE_JOB_BYIDX(data, njobs, structsz);
        job->tree = *tree;
        job->tree.flags |= AFL_DISABLE_PARALLEL; /* parallel visit forbidden in threads */
        job->tree.shared = NULL; /* use dedicated stack/context for thread visit */
        job->tree.root = NULL;
        if (userdatasz > 0) {
            job->job_user_data = ((unsigned char *)&(job->job_user_data))
                                  + sizeof(job->job_user_data);
            memcpy(job->job_user_data, user_data, userdatasz);
        } else {
            job->job_user_data = user_data;
        }
        job->common = &common_data;
        job->job = NULL;
        job->parent = NULL; /* there cannot be more parents than number of jobs */
    }

    /* perform a width (breadth style) visit stopping when there is enough leafs for CPUs */
    njobs = 0;
    nparents = 0;
    if (tree->root != NULL) {
        rbuf_push(fifo, tree->root);
        rbuf_push(fifo, (void*)(0L));
    }

    while (rbuf_size(fifo) != 0) {
        node = rbuf_dequeue(fifo);
        depth = (unsigned int)((unsigned long)rbuf_dequeue(fifo));

        LOG_SCREAM(g_vlib_log, "avltree_parallel: node %ld depth %u", (long)node->data, depth);

        if (1U << depth >= ncpus) { /* we reached a suitable depth to run jobs */
            /* Launch multi-threaded visit */
            avltree_visit_parallel_t * job = AVLTREE_JOB_BYIDX(data, njobs, structsz);

            LOG_DEBUG(g_vlib_log, "avltree_parallel: node %ld, RUNNING visit job#%u",
                      (long)node->data, njobs);

            job->tree.root = node;
            if ((job->job = vjob_run(avltree_visit_job, job)) == NULL) {
                ret = AVS_ERROR;
            }
            ++njobs;
        } else {
            /* keep parent of pushed nodes */
            avltree_visit_parallel_t * par = AVLTREE_JOB_BYIDX(data, nparents++, structsz);
            par->parent = node;

            if (node->left != NULL) {
                rbuf_push(fifo, node->left);
                rbuf_push(fifo, (void*)((unsigned long) (depth + 1)));
            }

            if (node->right != NULL) {
                rbuf_push(fifo, node->right);
                rbuf_push(fifo, (void*)((unsigned long) (depth + 1)));
            }
        }
    }

    LOG_DEBUG(g_vlib_log, "avltree_parallel: %u job%s launched, %u parent%s",
              njobs, njobs > 1 ? "s": "", nparents, nparents > 1 ? "s" : "");

    /* visit parents of nodes visited by threads */
    /* TODO: with AVH_MERGE, visit parents and childs roots in requested order(how) */
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = AVH_PREFIX;
    ctx->how = how;
    ctx->data = NULL;

    while (nparents-- > 0) {
        avltree_visit_parallel_t * par = AVLTREE_JOB_BYIDX(data, nparents, structsz);
        node = par->parent;

        LOG_DEBUG(g_vlib_log, "avltree_parallel: parent visit - node %ld",
                  (long)node->data);

        if (visit(tree, node, ctx, user_data) == AVS_ERROR)
            ret = AVS_ERROR;
    }

    /* Wait and free jobs, merge results if necessary, by visiting roots of
     * jobs subtrees with AVH_MERGE if jobs have allocated data, they must free it now. */
    if ((how & AVH_MERGE) != 0) {
        ctx->state = AVH_MERGE;
        ctx->how = how;
        LOG_DEBUG(g_vlib_log, "avltree_parallel: wait child%s and merge",
                  (njobs > 1 ? "s" : ""));

    }
    while (njobs-- > 0) {
        /* wait */
        avltree_visit_parallel_t * job = AVLTREE_JOB_BYIDX(data, njobs, structsz);
        void * job_result = vjob_waitandfree(job->job);
        if ((long)job_result != AVS_FINISHED
        && ((long)job_result == AVS_ERROR
            || job_result == VJOB_ERR_RESULT || job_result == VJOB_NO_RESULT)) {
            ret = AVS_ERROR;
        }
        /* merge */
        if ((how & AVH_MERGE) != 0) {
            ctx->data = job->job_user_data;
            if (visit(tree, job->tree.root, ctx, user_data) == AVS_ERROR) {
                ret = AVS_ERROR;
            }
        }
    }

    /* free resources, restore stack/root and return */
    free(data);
    avltree_visit_resources_free(tree, fifo, ctx);

    LOG_DEBUG(g_vlib_log, "%s(): EXITING with status %d", __func__, ret);
    return ret;
}

/*****************************************************************************/
int                 avltree_visit(
                        avltree_t *                 tree,
                        avltree_visitfun_t          visit,
                        void *                      user_data,
                        avltree_visit_how_t         how) {
    avltree_visit_context_t*context;
    rbuf_t *                stack;
    int                     ret             = AVS_CONTINUE;
    int                     breadth_style   = 0;
    int                     push            = 1;

    if (tree == NULL || visit == NULL) {
        return AVS_ERROR;
    }
    /* enable parallel only if more than 1 cpu and tree big enough */
    if ((how & AVH_PARALLEL) != 0 && (tree->flags & AFL_DISABLE_PARALLEL) == 0
    && (ret = vjob_cpu_nb()) > 1
    &&  tree->n_elements > 0 && 1 << (avltree_find_depth(tree) - 1) >= ret) {
        return avltree_visit_parallel(tree, visit, user_data, how);
    }
    /* allocate context/stack if not shared and not used */
    if (avltree_visit_resources_get(tree, &stack, &context) != AVS_FINISHED) {
        return AVS_ERROR;
    }
    /* push the root node, in any */
    if (tree->root != NULL) {
        rbuf_push(stack, tree->root);
    }

    context->level = 0;
    context->index = 0;
    context->stack = stack;
    context->data = NULL;
    context->how = how;
    how &= (AVH_MASK & ~(AVH_PARALLEL) & ~(AVH_MERGE));
    context->state = (how & AVH_BREADTH) != 0 ? (how & ~AVH_RIGHT & AVH_BREADTH) : AVH_PREFIX;
    breadth_style = (context->state == AVH_BREADTH || (how & ~AVH_RIGHT) == AVH_PREFIX);

    while (rbuf_size(stack) != 0) {
        avltree_node_t *      node = (avltree_node_t *) ((how & AVH_BREADTH) != 0 ?
                                                            rbuf_dequeue(stack) :
                                                            rbuf_pop(stack));
        //TODO: loop (stack mode) can be optimized by not pushing the next node
        //      to be visited, just do node = node->left/right
        //TODO: skip states which are not required by visitor (push child earlier)
        //TODO: AVS_NEXTVISIT and AVS_SKIP might not work correctly

        avltree_node_t *      right = node->right;
        avltree_node_t *      left = node->left;

        LOG_SCREAM(g_vlib_log, "altree_visit(): node:%ld(%ld,%ld) state:%d how:%d "
                               "do_visit:%d last_ret:%d how_orig:%d",
                (long) node->data,
                node->left?(long)node->left->data:-1,
                node->right?(long)node->right->data:-1,
                context->state, how, (how &~AVH_RIGHT & context->state), ret, context->how);

        /* visit the current node if required, and update ret only if visitor is called */
        if ((how & ~AVH_RIGHT & context->state) == context->state) {
            ret = visit(tree, node, context, user_data);
            /* stop on error or when visit goal is accomplished */
            if (ret == AVS_ERROR || ret == AVS_FINISHED) {
                break ;
            }
            /* we remove the current visit type from set */
            if ((ret & AVS_NEXTVISIT) == AVS_NEXTVISIT) {
                how &= ~(context->state);
                push = 0;
            }
        }

        /* prepare next visit */
        if (breadth_style) {
            /* push left/right child if required */
            if (push && (node = avltree_visit_get_child(AGC_SECOND, ret, how, left,right)) != NULL) {
                rbuf_push(stack, node);
            }
            if (push && (node = avltree_visit_get_child(AGC_FIRST, ret, how, left,right)) != NULL) {
                rbuf_push(stack, node);
            }
            continue ;
        }
        switch (context->state) {
            case AVH_PREFIX:
                /* push current node, so that child can return to its parent */
                if ((ret & AVS_SKIP) == 0) {
                    rbuf_push(stack, node);
                }
                /* push first child if required (taking care of AVH_RIGHT modifier) */
                if (push
                &&  (node = avltree_visit_get_child(AGC_FIRST, ret, how, left, right)) != NULL) {
                    /* visit first child in prefix mode */
                    rbuf_push(stack, node);
                } else {
                    if (!push && (how & AVH_INFIX) == 0) {
                        /* stay on node for suffix visit, as neither infix nor push is required */
                        context->state = AVH_SUFFIX;
                    } else {
                        /* stay on node for infix visit */
                        context->state = AVH_INFIX;
                    }
                }
                break ;
            case AVH_INFIX:
                /* push current node, so that child can return to its parent */
                if ((ret & AVS_SKIP) == 0) {
                    rbuf_push(stack, node);
                }
                /* push left/right child if required (taking care of AVH_RIGHT modifier) */
                if (push
                &&  (node = avltree_visit_get_child(AGC_SECOND, ret, how, left, right)) != NULL) {
                    /* go to right, for prefix visit */
                    rbuf_push(stack, node);
                    context->state = AVH_PREFIX;
                } else {
                    /* stays on node, for SUFFIX visit */
                    context->state = AVH_SUFFIX;
                }
                break ;
            case AVH_SUFFIX:
                if ((ret & AVS_NEXTVISIT) == AVS_NEXTVISIT) {
                    break ;
                }
                if (rbuf_size(stack)) {
                    avltree_node_t * parent = rbuf_top(stack);
                    if ((push || (how & AVH_INFIX) != 0)
                    && node == avltree_visit_get_child(AGC_FIRST, AVS_CONTINUE, how,
                                                       parent->left, parent->right)) {
                        /* prepare the parent to be visited in infix mode */
                        context->state = AVH_INFIX;
                    } else {
                        context->state = AVH_SUFFIX;
                    }
                    /* next visit is for parent */
                }
                break ;
            default:
                LOG_ERROR(g_vlib_log, "avltree_visit: bad state %d", context->state);
                ret = AVS_ERROR;
                rbuf_reset(stack);
                break ;
        }
    }
    /* free resources if needed, and restore shared resources */
    avltree_visit_resources_free(tree, stack, context);

    LOG_DEBUG(g_vlib_log, "%s(): EXITING with status %d", __func__,
              ret != AVS_ERROR ? AVS_FINISHED : AVS_ERROR);
    return ret != AVS_ERROR ? AVS_FINISHED : AVS_ERROR;
}

/*****************************************************************************/
void *              avltree_remove(
                        avltree_t *                 tree,
                        const void *                data) {
    avltree_visit_insert_t    insert_data;

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (tree->root == NULL) {
        errno = ENOENT;
        return NULL;
    }
    insert_data.newdata = (void *) data;
    insert_data.newnode = NULL;
    insert_data.prev_nodep = &(tree->root);
    insert_data.new_balance = 0;
    if (avltree_visit(tree, avltree_visit_remove, &insert_data, AVH_PREFIX | AVH_SUFFIX)
            == AVS_FINISHED) {
        --tree->n_elements;
        if (insert_data.newdata == NULL) {
            errno = 0;
        }
        return insert_data.newdata;
    }
    errno = ENOENT;
    return NULL;
}

/*****************************************************************************/
int                 avltree_visit_range(
                        avltree_t *                 tree,
                        void *                      min,
                        void *                      max,
                        avltree_visitfun_t          visit,
                        void *                      user_data,
                        avltree_visit_how_t         how_RFU) {
    avltree_visit_range_t rangedata = { min, max, visit, user_data };
    (void) how_RFU;

    return avltree_visit(tree, avltree_visit_find_range, &rangedata, AVH_PREFIX | AVH_INFIX);
}

/*****************************************************************************/
typedef struct {
    slist_t * head;
    slist_t * tail;
} avltree_slist_data_t;
AVLTREE_DECLARE_VISITFUN(avltree_toslist_visit, tree, node, context, vdata) {
    avltree_slist_data_t * data = (avltree_slist_data_t *) vdata;
    (void) tree;
    if ((context->state & AVH_MERGE) != 0) {
        avltree_slist_data_t * child_data = ((avltree_slist_data_t *) context->data);
        if (data->head == NULL) {
            data->head = child_data->head;
            data->tail = child_data->tail;
        } else if (child_data->head != NULL) {
            data->tail->next = child_data->head;
            data->tail = child_data->tail;
        }
    } else {
        slist_t * new = slist_prepend(NULL, node->data);
        if (new == NULL) {
            return AVS_ERROR;
        }
        if (data->head == NULL) {
            data->head = new;
        } else {
            data->tail->next = new;
        }
        data->tail = new;
    }
    return AVS_CONTINUE;
}
/*****************************************************************************/
slist_t *           avltree_to_slist(
                        avltree_t *                 tree,
                        avltree_visit_how_t         how) {
    avltree_slist_data_t data = { .head = NULL, .tail = NULL };

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if ((how & AVH_PARALLEL) != 0) {
        how = AVH_PARALLEL_DUPDATA(how | AVH_MERGE, sizeof(data));
    }
    if (avltree_visit(tree, avltree_toslist_visit, &data, how) != AVS_FINISHED) {
        slist_free(data.head, NULL);
        errno = EFAULT;
        return NULL;
    }

    if (data.head == NULL) {
        errno = 0;
    }
    return data.head;
}

/*****************************************************************************/
AVLTREE_DECLARE_VISITFUN(avltree_torbuf_visit, tree, node, context, vdata) {
    rbuf_t ** prbuf = (rbuf_t **) vdata;
    (void) tree;

    if ((context->state & AVH_MERGE) != 0) {
        rbuf_t * child_rbuf = *((rbuf_t **) context->data);

        if (*prbuf == NULL) {
            *prbuf = child_rbuf;
        } else {
            for (size_t i = 0, n = rbuf_size(child_rbuf); i < n; ++i) {
                rbuf_push(*prbuf, rbuf_get(child_rbuf, i));
            }
            rbuf_free(child_rbuf);
        }
        *((rbuf_t**)(context->data)) = NULL;
    } else {
        if (*prbuf == NULL
        && NULL == (*prbuf = rbuf_create((tree->n_elements / vjob_cpu_nb()) + 1, RBF_DEFAULT))) {
            return AVS_ERROR;
        }
        if (rbuf_push(*prbuf, node->data) != 0) {
            return AVS_ERROR;
        }
    }
    return AVS_CONTINUE;
}
/*****************************************************************************/
rbuf_t *            avltree_to_rbuf(
                        avltree_t *                 tree,
                        avltree_visit_how_t         how) {
    rbuf_t * rbuf = NULL;

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if ((how & AVH_PARALLEL) != 0) {
        how = AVH_PARALLEL_DUPDATA(how | AVH_MERGE, sizeof(*(&rbuf)) );
    }
    if (avltree_visit(tree, avltree_torbuf_visit, &rbuf, how) != AVS_FINISHED) {
        rbuf_free(rbuf);
        errno = EFAULT;
        return NULL;
    }

    if (rbuf == NULL) {
        errno = 0;
    }
    return rbuf;
}

/*****************************************************************************/
typedef struct {
    void **     array;
    size_t      n;
    size_t      max;
} avltree_array_visit_data_t;
AVLTREE_DECLARE_VISITFUN(avltree_toarray_visit, tree, node, context, vdata) {
    avltree_array_visit_data_t * data = (avltree_array_visit_data_t *) vdata;
    (void) tree;
    (void) context;
    if (data->n >= data->max)
        return AVS_ERROR;
    data->array[(data->n)++] = node->data;
    return AVS_CONTINUE;
}
/*****************************************************************************/
size_t              avltree_to_array(
                        avltree_t *                 tree,
                        avltree_visit_how_t         how,
                        void ***                    parray) {
    size_t                      n;
    avltree_array_visit_data_t  data;

    if (tree == NULL || parray == NULL) {
        if (parray != NULL)
            *parray = NULL;
        errno = EINVAL;
        return 0;
    }

    if ((how & AVH_PARALLEL) != 0) {
        rbuf_t *    rbuf = NULL;

        if ((rbuf = avltree_to_rbuf(tree, how)) == NULL) {
            *parray = NULL;
            return 0;
        }
        if ((*parray = malloc(rbuf_size(rbuf) * sizeof(void *))) == NULL) {
            rbuf_free(rbuf);
            return 0;
        }
        n = rbuf_size(rbuf);
        for (size_t i = 0; i < n; ++i) {
            (*parray)[i] = rbuf_get(rbuf, i);
        }
        rbuf_free(rbuf);
        return n;
    }
    n = (((how & AVH_PREFIX) / AVH_PREFIX) + ((how & AVH_INFIX) / AVH_INFIX)
      +  ((how & AVH_SUFFIX) / AVH_SUFFIX) + ((how & AVH_BREADTH) / AVH_BREADTH)
         ) * tree->n_elements;
    if ((*parray = malloc(n * sizeof(*parray))) == NULL) {
        return 0;
    }
    data.n = 0;
    data.max = n;
    data.array = *parray;

    if (avltree_visit(tree, avltree_toarray_visit, &data, how) != AVS_FINISHED) {
        free(*parray);
        *parray = NULL;
        errno = EFAULT;
        return 0;
    }
    return data.n;
}

/*****************************************************************************/
#ifdef _DEBUG
# define _DEBUG_PRINT_TREE(tree) \
        if (g_vlib_log != NULL && LOG_CAN_LOG(g_vlib_log, LOG_LVL_SCREAM)   \
        && avltree_count(tree) < 32) {                                      \
            avltree_shared_t * shared = tree->shared; tree->shared = NULL;  \
            avltree_print(tree, avltree_print_node_default, g_vlib_log->out); \
            tree->shared = shared;                                          \
        }
#else
# define _DEBUG_PRINT_TREE(tree)
#endif
static int          avltree_visit_rebalance(
                        avltree_t *                     tree,
                        avltree_node_t *                node,
                        const avltree_visit_context_t * context,
                        void *                          user_data) {

    avltree_visit_insert_t *    idata       = (avltree_visit_insert_t *) user_data;
    avltree_node_t *            parent      = rbuf_top(context->stack); /* suffix mode, top=parent */
    avltree_node_t **           pparent;
    int                         balance_dir = &(node->left) == idata->prev_nodep ? -1 : +1;
    (void) context;

    pparent = parent ? (parent->left == node ? &parent->left : &parent->right) : &tree->root;

    LOG_SCREAM(g_vlib_log, "BALANCING %ld(%ld,%ld) ppar=%ld "
                           "difbal:%d dirbal:%d obal:%d nbal:%d (%s:%ld)",
                (long) node->data,
                node->left ? (long) node->left->data : -1,
                node->right ? (long) node->right->data : -1,
                parent ? (long)(*pparent)->data : -1,
                idata->new_balance, balance_dir,
                node->balance,
                node->balance + (balance_dir * idata->new_balance),
                idata->new_balance < 0 ? "Deleted" : "Inserted",
                (long) idata->newdata);

    int old_balance = node->balance;
    if (idata->new_balance >= 0) {
        node->balance += (balance_dir * idata->new_balance);
        if (balance_dir == -1 && old_balance <= 0) {
            /* left subtree increases */
        } else if (balance_dir == 1 && old_balance >= 0) {
            /* right subtree increases */
        } else {
            return AVS_FINISHED;
        }
    } else if (idata->new_balance < 0) {
        node->balance += (balance_dir * idata->new_balance);
        if (balance_dir == -1 && old_balance > 0) {
            /* left subtree decreases */
        } else if (balance_dir == 1 && old_balance < 0) {
            /* right subtree decreases */
        } else if (old_balance == 0) {
            return AVS_FINISHED;
        }
    }

    if (node->balance < -1) {
        _DEBUG_PRINT_TREE(tree);
        /* right rotate */
        if ((old_balance = node->left->balance) > 0) {
            //double rotate
            LOG_DEBUG(g_vlib_log, "left(%ld) right(%ld) rotation",
                                  (long)node->left->data, (long) node->data);
            *pparent = avltree_rotate_leftright(tree, node);
        } else {
            LOG_DEBUG(g_vlib_log, "right(%ld) rotation", (long) node->data);
            *pparent = avltree_rotate_right(tree, node, 1);
        }
        idata->prev_nodep = pparent;
        if (idata->new_balance >= 0 || old_balance == 0)
            return AVS_FINISHED;
    } else if (node->balance > 1) {
        _DEBUG_PRINT_TREE(tree);
        // left rotate
        if ((old_balance = node->right->balance) < 0) {
            //double rotation
            LOG_DEBUG(g_vlib_log, "right(%ld) left(%ld) rotation",
                                  (long)node->right->data, (long) node->data);
            *pparent = avltree_rotate_rightleft(tree, node);
        } else {
            LOG_DEBUG(g_vlib_log, "left(%ld) rotation", (long) node->data);
            *pparent = avltree_rotate_left(tree, node, 1);
        }
        idata->prev_nodep = pparent;
        if (idata->new_balance >= 0 || old_balance == 0)
            return AVS_FINISHED;
    }
    idata->prev_nodep = pparent;

    return AVS_CONTINUE;
}

/*****************************************************************************/
static int          avltree_visit_insert(
                        avltree_t *                     tree,
                        avltree_node_t *                node,
                        const avltree_visit_context_t * context,
                        void *                          user_data) {
    avltree_visit_insert_t *    idata = (avltree_visit_insert_t *) user_data;
    avltree_node_t *            new;
    avltree_node_t **           parent;
    int                         cmp;

    if (context->state == AVH_SUFFIX) {
        return avltree_visit_rebalance(tree, node, context, user_data);
    } else if (context->state != AVH_PREFIX) {
        return AVS_ERROR;
    }

    LOG_SCREAM(g_vlib_log, "INSERTING %ld, Visiting Node %ld(%ld,%ld)"
                           " State:%d How:%d Ptr %lx(%lx,%lx)",
              (long)idata->newdata, (long)node->data,
              node->left?(long)node->left->data:-1, node->right?(long)node->right->data:-1,
              context->state, context->how,
              (unsigned long) node, (unsigned long)node->left, (unsigned long)node->right);

    cmp = tree->cmp(idata->newdata, node->data);
    if (cmp == 0 && (cmp = (tree->flags & AFL_INSERT_MASK)) != 0) {
        if (cmp == AFL_INSERT_NODOUBLE) {
            /* having doubles is forbidden in this tree: return error */
            LOG_DEBUG(g_vlib_log, "%s(): %lx already in tree, keep it - "
                                  "node %ld(%lx,%lx) ptr 0x%lx", __func__,
              (long)(idata->newdata),
              (long)node->data, (unsigned long)node->left, (unsigned long)node->right,
              (unsigned long) node);

            return AVS_ERROR;
        }
        /* replace or ignore exsting element: previous one is returned */
        void * prevdata = node->data;
        if (cmp == AFL_INSERT_REPLACE) {
            /* replace data in the node, return previous data, and free it if needed */
            LOG_DEBUG(g_vlib_log, "%s(): %lx already in tree, replace it - "
                                  "node %ld(%lx,%lx) ptr 0x%lx", __func__,
              (long)(idata->newdata),
              (long)node->data, (unsigned long)node->left, (unsigned long)node->right,
              (unsigned long) node);

            node->data = idata->newdata;
            if (tree->free != NULL && (tree->flags & AFL_REMOVE_NOFREE) == 0) {
                tree->free(prevdata);
            }
        }
        idata->newnode = node;
        idata->new_balance = 0;
        idata->newdata = prevdata;
        return AVS_FINISHED;
    } else if (cmp <= 0) {
        /* go left */
        if (node->left != NULL) {
            /* continue with left node */
            return AVS_GO_LEFT;
        }
        /* left is null, we insert on node->left */
        parent = &(node->left);
    } else {
        /* go right */
        if (node->right != NULL) {
            /* continue with right node */
            return AVS_GO_RIGHT;
        }
        /* right is null, we insert on node->right */
        parent = &(node->right);
    }
    /* Node Creation */
    new = avltree_node_alloc(tree);
    if (new == NULL) {
        return AVS_ERROR;
    }
    new->left = NULL;
    new->right = NULL;
    new->data = idata->newdata;
    new->balance = 0;
    (*parent) = new;
    idata->newnode = new;
    idata->prev_nodep = parent;
    idata->new_balance = 1;

    LOG_DEBUG(g_vlib_log, "%s(): INSERTED node %ld(%lx,%lx) ptr 0x%lx on %s of %ld ptr 0x%lx",
              __func__,
              (long)new->data, (unsigned long)new->left, (unsigned long)new->right,
              (unsigned long) new,
              parent == &(node->left) ? "LEFT" : "RIGHT",
              (long)node->data, (unsigned long) node);

    /* stop prefix visit and switch to suffix visit */
    return AVS_NEXTVISIT;
}

/****************************************************************************
 * The method proposed by T. Hibbard's in 1962 is used
 * (heights of subtrees changed by at most 1):
 * 1) deleting a node with no children: remove the node
 * 2) deleting a node with one child: remove the node and replace it with its child
 * 3) deleting a node(N) with 2 children: do not delete N. Choose its infix successor or
 *    infix predecessor (R) as replacement, Copy value of R to N.
 *    a) If R does not have child, remove R from its parent.
 *    b) if R has a child (C, right child), replace R with C at R's parent.
 * When D is root, make R root.
 * infix successor:   right subtree's left-most  child
 * infix predecessor: left  subtree's right-most child */
static int              avltree_visit_remove(
                            avltree_t *                     tree,
                            avltree_node_t *                node,
                            const avltree_visit_context_t * context,
                            void *                          user_data) {
    avltree_visit_insert_t *    idata = (avltree_visit_insert_t *) user_data;
    avltree_node_t **           parent;
    int                         cmp;

    if (context->state == AVH_SUFFIX) {
        return avltree_visit_rebalance(tree, node, context, user_data);
    } else if (context->state != AVH_PREFIX) {
        return AVS_ERROR;
    }

    LOG_SCREAM(g_vlib_log, "DELETING %ld, Visiting Node %ld(%ld,%ld)"
                           " State:%d How:%d Ptr %lx(%lx,%lx)",
              (long)idata->newdata, (long)node->data,
              node->left?(long)node->left->data:-1, node->right?(long)node->right->data:-1,
              context->state, context->how,
              (unsigned long) node, (unsigned long)node->left, (unsigned long)node->right);

    /* keep the parent of node : works because we are in prefix mode */
    parent = idata->prev_nodep;

    /* compare node value with searched one */
    if ((cmp = tree->cmp(idata->newdata, node->data)) < 0) {
        /* continue with left node, or raise error if NULL */
        if (node->left == NULL) {
            return AVS_ERROR;
        }
        idata->prev_nodep = &(node->left);
        return AVS_GO_LEFT;
    } else if (cmp > 0) {
        /* continue with right node, or raise error if NULL */
        if (node->right == NULL) {
            return AVS_ERROR;
        }
        idata->prev_nodep = &(node->right);
        return AVS_GO_RIGHT;
    }
    /* Found. Node Deletion.
     * When D is root, make R root : done with (*parent) = ...,
     *   (prev_nodep set to root in avltree_remove). */
    idata->newdata = node->data;
    if (node->left == NULL && node->right == NULL) {
        /* 1) deleting a node with no children: remove the node */
        LOG_DEBUG(g_vlib_log, "avltree_remove(%ld): case 1 (no child)", (long)(idata->newdata));
        (*parent) = NULL;
        avltree_node_free(tree, node, (tree->flags & AFL_REMOVE_NOFREE) == 0);
        idata->newnode = node;
        idata->prev_nodep = parent;
    } else if (node->left == NULL || node->right == NULL) {
        /* 2) deleting a node with one child: remove the node and replace it with its child */
        LOG_DEBUG(g_vlib_log, "avltree_remove(%ld): case 2 (1 child)", (long)(idata->newdata));
        (*parent) = (node->left != NULL ? node->left : node->right);
        avltree_node_free(tree, node, (tree->flags & AFL_REMOVE_NOFREE) == 0);
        idata->newnode = node;
        idata->prev_nodep = parent;
    } else {
        /* 3) deleting a node(N) with 2 children: do not delete N:
         *    choose infix successor as replacement (R), copy value of R to N.*/
        rbuf_push(context->stack, node);
        avltree_node_t ** preplace = &(node->right), * replace;
        while ((*preplace)->left != NULL) {
            rbuf_push(context->stack, *preplace);
            preplace = &((*preplace)->left);
        }
        replace = *preplace;
        idata->prev_nodep = preplace;
        LOG_DEBUG(g_vlib_log, "avltree_remove(%ld): case 3%c (2 children), replace:%ld(%ld:%ld)",
                  (long)(node->data), replace->right == NULL ? 'a' : 'b',
                  (long)(replace->data),
                  replace->left ? (long) (replace->left->data) : -1l,
                  replace->right ? (long) (replace->right->data) : -1l);

        /* a&b) if R has a child (C,right child), replace R with C at R's parent, else remove it. */
        node->data = replace->data;
        replace->data = idata->newdata;
        *preplace = replace->right;
        avltree_node_free(tree, replace, (tree->flags & AFL_REMOVE_NOFREE) == 0);
        idata->newnode = replace;
    }

    idata->new_balance = -1;
    LOG_DEBUG(g_vlib_log, "%s(): DELETED node %ld (0x%p)", __func__,
              (long)idata->newdata, (void *) idata->newnode);

    /* skip this node and stop prefix visit and switch to suffix visit */
    return AVS_NEXTVISIT | AVS_SKIP;
}

/*****************************************************************************/
static int          avltree_visit_free(
                        avltree_t *                     tree,
                        avltree_node_t *                node,
                        const avltree_visit_context_t * context,
                        void *                          user_data) {
    (void) context;
    (void) user_data;

    avltree_node_free(tree, node, 1);
    return AVS_CONTINUE;
}

/*****************************************************************************/
static int              avltree_visit_find_range(
                            avltree_t *                     tree,
                            avltree_node_t *                node,
                            const avltree_visit_context_t * context,
                            void *                          user_data) {
    avltree_visit_range_t * rangedata = (avltree_visit_range_t *) user_data;

    if (context->state == AVH_PREFIX) {
        if (tree->cmp(node->data, rangedata->min) < 0) {
            return AVS_GO_RIGHT;
        }
        return AVS_GO_LEFT;
    } else if (context->state == AVH_INFIX) {
        if (tree->cmp(node->data, rangedata->min) < 0) {
            return AVS_CONTINUE;
        }
        if (tree->cmp(node->data, rangedata->max) > 0) {
            return AVS_FINISHED;
        }
        if (rangedata->visit == NULL) {
            return AVS_CONTINUE;
        }
        return rangedata->visit(tree, node, context, rangedata->user_data);
    }
    return AVS_ERROR;
}

/*****************************************************************************/
static avltree_node_t * avltree_rotate_left(avltree_t * tree, avltree_node_t * node, int update) {
    (void) tree;
    avltree_node_t * rchild = node->right;

    node->right = rchild->left;
    rchild->left = node;

    if (update == 1) {
        if (rchild->balance == 0) {
            node->balance = 1;
            rchild->balance = -1;
        } else {
            node->balance = 0;
            rchild->balance = 0;
        }
    }
    return rchild;
}
/*****************************************************************************/
static avltree_node_t * avltree_rotate_right(avltree_t * tree, avltree_node_t * node, int update) {
    (void) tree;
    avltree_node_t * lchild = node->left;

    node->left = lchild->right;
    lchild->right = node;

    if (update == 1) {
        if (lchild->balance == 0) {
            node->balance = -1;
            lchild->balance = 1;
        } else {
            node->balance = 0;
            lchild->balance = 0;
        }
    }
    return lchild;
}
/*****************************************************************************/
static avltree_node_t * avltree_rotate_rightleft(avltree_t * tree, avltree_node_t * node) {
    (void) tree;
    avltree_node_t * rchild = node->right;
    avltree_node_t * l_of_rchild = rchild->left;
    avltree_node_t * newroot;

    node->right = avltree_rotate_right(tree, node->right, 0);
    newroot = avltree_rotate_left(tree, node, 0);

    if (l_of_rchild->balance > 0) {
        node->balance = -1;
        rchild->balance = 0;
    } else if (l_of_rchild->balance == 0) {
        node->balance = 0;
        rchild->balance = 0;
    } else {
        node->balance = 0;
        rchild->balance = 1;
    }
    l_of_rchild->balance = 0;

    return newroot;
}
/*****************************************************************************/
static avltree_node_t * avltree_rotate_leftright(avltree_t * tree, avltree_node_t * node) {
    (void) tree;
    avltree_node_t * lchild = node->left;
    avltree_node_t * r_of_lchild = lchild->right;
    avltree_node_t * newroot;

    node->left = avltree_rotate_left(tree, node->left, 0);
    newroot = avltree_rotate_right(tree, node, 0);

    if (r_of_lchild->balance < 0) {
        node->balance = 1;
        lchild->balance = 0;
    } else if (r_of_lchild->balance == 0) {
        node->balance = 0;
        lchild->balance = 0;
    } else {
        node->balance = 0;
        lchild->balance = -1;
    }
    r_of_lchild->balance = 0;

    return newroot;
}

/*****************************************************************************/
int                 avltree_print_node_default(
                        FILE *                      out,
                        const avltree_node_t *      node) {
    return fprintf(out, "%ld(%d)", (long)node->data, node->balance);
}

/*****************************************************************************/
/* TODO, the avltree_visit is not ready to pass node index, node depth,
 * TODO, get terminal columns number (width)
 * then for now the visit is inlined in this function */
void avltree_print(avltree_t * tree, avltree_printfun_t print, FILE * out) {
    avltree_visit_context_t * context;
    rbuf_t *    fifo;
    int         node_nb     = 1;
    int         width;
    int         node_sz;
    int         display;
    int         indent;
    ssize_t     n;
    ssize_t     old_idx     = -1;

    if (tree == NULL || out == NULL
    ||  avltree_visit_resources_get(tree, &fifo, &context) != AVS_FINISHED) {
        return ;
    }

    if ((width = vterm_get_columns(fileno(out))) == 0)
        width = 100; /* not a tty */
    else if (width < 80)
        width = 80; /* minimum width */
    node_nb     = 1;
    node_sz     = width / 3;
    display     = (node_nb - 1) * node_sz;
    indent      = (width - display) / 2;

    if (tree && tree->root) {
        rbuf_push(fifo, 0);
        rbuf_push(fifo, 0);
        rbuf_push(fifo, tree->root);
        for (n = 0; n < indent; n++)
            fputc(' ', out);
    }

    while (rbuf_size(fifo)) {
        size_t              level   = (size_t) rbuf_dequeue(fifo);
        ssize_t             idx     = (ssize_t) rbuf_dequeue(fifo);
        avltree_node_t *    node    = (avltree_node_t *) rbuf_dequeue(fifo);

        /* padding for inexistant previous brothers */
        if (old_idx + 1 < idx) {
            for(n = 0; n < node_sz * (idx - old_idx - 1); n++)
                fputc(' ', out);
        }

        /* print node & padding */
        n = print(out, node);
        if (rbuf_size(fifo) != 0 && (size_t) rbuf_bottom(fifo) == level) {
            for( ; n < node_sz; n++)
                fputc(' ', out);
        }
        /* detect if this is the last element of the level */
        if (rbuf_size(fifo) == 0 || level != (size_t) rbuf_bottom(fifo)) {
            /* next element is on new level */
            old_idx = -1;
            fputc('\n', out);
            node_nb = 1 << (level + 1);
            node_sz = width / (node_nb);
            display = (node_nb - 1) * node_sz;
            indent = (width - display) / 2;
            for (n = 0; n < indent; n++)
                    fputc(' ', out);
            for (int i = 0; i < (1 << (level)); i++) {
                for (n = 0; n < node_sz; n++)
                    fputc(n == node_sz / 2 ? '+' : '_', out);
                if (i + 1 != (1 << (level)))
                    for (n = 0; n < node_sz; n++)
                        fputc(' ', out);
            }
            fputc('\n', out);
            for (n = 0; n < indent; n++)
                fputc(' ', out);
        } else {
            old_idx = idx;
        }

        if (node->left) {
            rbuf_push(fifo, (void*)((long)(level + 1)));
            rbuf_push(fifo, (void*)((ssize_t)(idx * 2)));
            rbuf_push(fifo, node->left);
        }
        if (node->right) {
            rbuf_push(fifo, (void*)((long)(level + 1)));
            rbuf_push(fifo, (void*)((ssize_t)(idx * 2 + 1)));
            rbuf_push(fifo, node->right);
        }
    }
    fputc('\n', out);

    avltree_visit_resources_free(tree, fifo, context);
}

