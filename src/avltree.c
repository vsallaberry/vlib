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
#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#if !defined(VLIB_AVLTREE_NODE_TESTS)
# define VLIB_AVLTREE_NODE_TESTS 1
#endif
#include "vlib/avltree.h"
#include "vlib/log.h"
#include "vlib/rbuf.h"
#include "vlib/job.h"
#include "vlib/term.h"
#include "vlib/util.h"

#include "vlib_private.h"

/*****************************************************************************/
#define AVLTREE_STACK_SZ    32 /* 10^5 elts:depth=20, 10^6:24, 10^7:28 */

#if ! defined(AVLTREE_OPTIMIZE_BITS)
# define AVLTREE_OPTIMIZE_BITS 1
#endif
//#define AVLTREE_POSIX_MEMALIGN_FALLBACK

/*****************************************************************************/
/** resources shared between several trees */
struct avltree_shared_s {
    struct rbuf_s *             stack;
    avltree_visit_context_t *   context;
    avltree_iterator_t *        iterator;
    pthread_mutex_t             mutex;
    int                         in_use;
};

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

/** avltree_iterator_t */
struct avltree_iterator_s {
    avltree_visit_context_t *   context;
    rbuf_t *                    stack;
    void *                      (*destack_fun)(rbuf_t *);
    avltree_visit_how_t         how;
    int                         allocated:1;
    int                         breadth_style:1;
    int                         push:1;
    int                         range:1;
    int                         invert_childs:1;
    void *                      min;
    void *                      max;
};

#if AVLTREE_OPTIMIZE_BITS
/* use the pointer values (given by malloc) bit0 to store balance */
/*
 * USING POINTER BIT0 -> balance is stored in left_ and right_
 */

/** opaque avltree_node_t: !! NEVER access these fields directly !! (use AVL_LEFT(),...) */
struct avltree_node_s {
    uintptr_t                   left_;      /* (struct avltree_node_s *) */
    uintptr_t                   right_;     /* (struct avltree_node_s *) */
    void *                      data_;
};

# define AVL_NODEMASK(_node)             ( (_node) & ~1UL )
# define AVL_LEFT(_node)                 ( (avltree_node_t *) AVL_NODEMASK((_node)->left_)  )
# define AVL_RIGHT(_node)                ( (avltree_node_t *) AVL_NODEMASK((_node)->right_) )
# define AVL_LEFT_PTR(_node)             ( (void *) &((_node)->left_) )
# define AVL_RIGHT_PTR(_node)            ( (void *) &((_node)->right_) )
# define AVL_BALANCE(_node)              ( (char) (((_node)->left_ & 1UL) << 1 | ((_node)->right_ & 1UL) ) - 2)
# define AVL_DATA(_node)                 ( (_node)->data_ )

# define AVL_SET_LEFT(_node, _left)      ( (_node)->left_  = ((_node)->left_ & 1UL)  | AVL_NODEMASK((uintptr_t)_left) )
# define AVL_SET_RIGHT(_node, _right)    ( (_node)->right_ = ((_node)->right_ & 1UL) | AVL_NODEMASK((uintptr_t)_right) )
# define AVL_SET_BALANCE(_node,_balance) ( (_node)->right_ = AVL_NODEMASK((_node)->right_) | (((_balance)  + 2UL) & 0x01UL), \
                                           (_node)->left_  = AVL_NODEMASK((_node)->left_)  | ((((_balance) + 2UL) & 0x02UL) >> 1) )
# define AVL_SET_DATA(_node, _data)      ( (_node)->data_  = (_data) )
# define AVL_NODE(_nodeptr)              ( (avltree_node_t *)(AVL_NODEMASK((uintptr_t)*(_nodeptr))) )
# define AVL_SET_NODE(_nodeptr, _new)    ( *(_nodeptr) = (avltree_node_t *)((((uintptr_t)*(_nodeptr)) & 1UL) | (uintptr_t)(_new)))

#else
/*
 * NOT USING MALLOC BIT0 -> field balance in avltree_node_t
 */

/** opaque avltree_node_t: !! NEVER access these fields directly !! (use AVL_LEFT(),...) */
struct avltree_node_s {
    struct avltree_node_s *     left_;
    struct avltree_node_s *     right_;
    char                        balance_;
    void *                      data_;
};

# define AVL_NODEMASK(_node)             ( (_node) )
# define AVL_LEFT(_node)                 ( (_node)->left_ )
# define AVL_RIGHT(_node)                ( (_node)->right_ )
# define AVL_LEFT_PTR(_node)             ( &((_node)->left_) )
# define AVL_RIGHT_PTR(_node)            ( &((_node)->right_) )
# define AVL_BALANCE(_node)              ( (_node)->balance_ )
# define AVL_DATA(_node)                 ( (_node)->data_ )

# define AVL_SET_LEFT(_node, _left)      ( (_node)->left_ = (_left) )
# define AVL_SET_RIGHT(_node, _right)    ( (_node)->right_ = (_right) )
# define AVL_SET_BALANCE(_node,_balance) ( (_node)->balance_ = (_balance) )
# define AVL_SET_DATA(_node, _data)      ( (_node)->data_ = (_data) )
# define AVL_NODE(_nodeptr)              ( *(_nodeptr) )
# define AVL_SET_NODE(_nodeptr, _new)    ( *(_nodeptr) = _new )

#endif // ! #if AVLTREE_OPTIMIZE_BITS

/*****************************************************************************/
static avltree_node_t * avltree_rotate_left(avltree_t * tree, avltree_node_t * node, int update);
static avltree_node_t * avltree_rotate_right(avltree_t * tree, avltree_node_t * node, int update);
static avltree_node_t * avltree_rotate_rightleft(avltree_t * tree, avltree_node_t * node);
static avltree_node_t * avltree_rotate_leftright(avltree_t * tree, avltree_node_t * node);

static AVLTREE_DECLARE_VISITFUN(avltree_visit_rebalance, node_data, context, user_data);
static AVLTREE_DECLARE_VISITFUN(avltree_visit_insert, node_data, context, user_data);
static AVLTREE_DECLARE_VISITFUN(avltree_visit_remove, node_data, context, user_data);
static AVLTREE_DECLARE_VISITFUN(avltree_visit_free, node_data, context, user_data);
static AVLTREE_DECLARE_VISITFUN(avltree_visit_find_range, node_data, context, user_data);

static inline int   avltree_iterator_init(
                        avltree_iterator_t *        iterator,
                        avltree_t *                 tree,
                        avltree_visit_how_t         how);
static inline void  avltree_iterator_clean(avltree_iterator_t * iterator);

/*****************************************************************************/
#if AVLTREE_OPTIMIZE_BITS && defined(AVLTREE_POSIX_MEMALIGN_FALLBACK)
static inline avltree_node_t * avltree_node_alloc(avltree_t * tree) {
    (void)tree;
    void * ptr = malloc(sizeof(avltree_node_t));
    if (((uintptr_t)ptr & 0x1) != 0) {
        free(ptr);
        if (posix_memalign(&ptr, sizeof(void*) * 2, sizeof(avltree_node_t)) == 0)
            return ptr;
        return NULL;
    }
    return ptr;
}
#else
static inline avltree_node_t * avltree_node_alloc(avltree_t * tree) {
    (void)tree;
    return malloc(sizeof(avltree_node_t));
}
#endif

/*****************************************************************************/
avltree_node_t *    avltree_node_create(
                        avltree_t *                 tree,
                        void *                      data,
                        avltree_node_t *            left,
                        avltree_node_t *            right) {
    avltree_node_t * new = avltree_node_alloc(tree);

    if (new == NULL)
        return NULL;
    AVL_SET_BALANCE(new, 0);
    AVL_SET_LEFT(new, left);
    AVL_SET_RIGHT(new, right);
    AVL_SET_DATA(new, data);

    return new;
}

/*****************************************************************************/
static inline void avltree_node_free(
                        avltree_t * tree, avltree_node_t * node, int freedata) {
    if (tree && node) {
        if (freedata && tree->free) {
            tree->free(AVL_DATA(node));
        }
        free(node);
    }
}

// ***************************************************************************
size_t              avltree_node_size() {
    return sizeof(avltree_node_t);
}

// ***************************************************************************
avltree_node_t *    avltree_node_left(const avltree_node_t * node) {
    return AVL_LEFT(node);
}

// ***************************************************************************
avltree_node_t *    avltree_node_right(const avltree_node_t * node) {
    return AVL_RIGHT(node);
}

// ***************************************************************************
char                avltree_node_balance(const avltree_node_t * node) {
    return AVL_BALANCE(node);
}

// ***************************************************************************
void *              avltree_node_data(const avltree_node_t * node) {
    return AVL_DATA(node);
}

// ***************************************************************************
void                avltree_node_set_data(avltree_node_t * node, void * data) {
    AVL_SET_DATA(node, data);
}

#if VLIB_AVLTREE_NODE_TESTS
// ** FOR TESTS ONLY *********************************************************
avltree_node_t **   avltree_node_left_ptr(avltree_node_t * node) {
    return AVL_LEFT_PTR(node);
}

// ***************************************************************************
avltree_node_t **   avltree_node_right_ptr(avltree_node_t * node) {
    return AVL_RIGHT_PTR(node);
}

// ***************************************************************************
void avltree_node_set(avltree_node_t ** nodeptr, avltree_node_t * newnode) {
    AVL_SET_NODE(nodeptr, newnode);
}

// ***************************************************************************
avltree_node_t *    avltree_node_get(avltree_node_t ** nodeptr) {
    return AVL_NODE(nodeptr);
}

// ***************************************************************************
void avltree_node_infos(avltree_node_info_t * infos) {
    if (!infos)
        return;
    infos->node_size = avltree_node_size();
    infos->iterator_size = sizeof(avltree_iterator_t);
    infos->left_offset = VLIB_OFFSETOF(avltree_node_t, left_);
    infos->right_offset = VLIB_OFFSETOF(avltree_node_t, right_);
    infos->data_offset = VLIB_OFFSETOF(avltree_node_t, data_);
    infos->optimize_bits = AVLTREE_OPTIMIZE_BITS;
#if AVLTREE_OPTIMIZE_BITS
    infos->balance_offset = 0;
#else
    infos->balance_offset = VLIB_OFFSETOF(avltree_node_t, balance_);
#endif
#if AVLTREE_OPTIMIZE_BITS && defined(AVLTREE_POSIX_MEMALIGN_FALLBACK)
    infos->posix_memalign_fallback = 1;
#else
    infos->posix_memalign_fallback = 0;
#endif
}
#endif // ! #if VLIB_AVLTREE_NODE_TESTS

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
        tree->shared->iterator = NULL;
        if (NULL == (tree->shared->stack
                    = rbuf_create(AVLTREE_STACK_SZ, RBF_DEFAULT | RBF_SHRINK_ON_RESET))
        || NULL == (tree->shared->context = malloc(sizeof(*(tree->shared->context))))
        || pthread_mutex_init(&(tree->shared->mutex), NULL) != 0) {
            rbuf_free(tree->shared->stack);
            if (tree->shared->context != NULL)
                free(tree->shared->context);
            if (tree->shared->iterator)
                free(tree->shared->iterator);
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
                  (unsigned long)tree->root, (long)AVL_DATA(tree->root),
                  (unsigned long)AVL_LEFT(tree->root), (unsigned long)AVL_RIGHT(tree->root));
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
        if (tree->shared->iterator != NULL)
            free(tree->shared->iterator);
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
        cmp = tree->cmp(data, AVL_DATA(node));
        if (cmp == 0) {
            if (AVL_DATA(node) == NULL) {
                errno = 0;
            }
            *find_context = AVL_LEFT(node); /* next one if any is on left child */
            return AVL_DATA(node);
        } else if (cmp < 0) {
            node = AVL_LEFT(node);
        } else {
            node = AVL_RIGHT(node);
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
    avltree_node_t *    node, * left;

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (tree->root == NULL) {
        errno = ENOENT;
        return NULL;
    }
    node = tree->root;
    while ((left = AVL_LEFT(node)) != NULL) {
        node = left;
    }
    if (AVL_DATA(node) == NULL) {
        errno = 0;
    }
    return AVL_DATA(node);
}

/*****************************************************************************/
void *              avltree_find_max(
                        avltree_t *                 tree) {
    avltree_node_t *    node, * right;

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (tree->root == NULL) {
        errno = ENOENT;
        return NULL;
    }
    node = tree->root;
    while ((right = AVL_RIGHT(node)) != NULL) {
        node = right;
    }
    if (AVL_DATA(node) == NULL) {
        errno = 0;
    }
    return AVL_DATA(node);
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
        if (AVL_BALANCE(node) > 0) {
            node = AVL_RIGHT(node);
        } else {
            node = AVL_LEFT(node);
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
        if (tree->shared->iterator != NULL) {
            size += sizeof(*(tree->shared->iterator));
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
                                    avltree_iterator_t *        it,
                                    avltree_node_t *            left,
                                    avltree_node_t *            right) {
    /* invert [ret, left and right] if the visit is from right to left 
     * and take care of return value (do you want to go left/right/both?) */    
    if (what == (it->invert_childs ? AGC_SECOND : AGC_FIRST)) {
        if ((ret & AVS_GO_LEFT) != 0) {
            return left;
        }
    } else if ((ret & AVS_GO_RIGHT) != 0) {
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

        LOG_SCREAM(g_vlib_log, "avltree_parallel: node %ld depth %u", (long)AVL_DATA(node), depth);

        if (1U << depth >= ncpus) { /* we reached a suitable depth to run jobs */
            /* Launch multi-threaded visit */
            avltree_visit_parallel_t * job = AVLTREE_JOB_BYIDX(data, njobs, structsz);

            LOG_DEBUG(g_vlib_log, "avltree_parallel: node %ld, RUNNING visit job#%u",
                      (long)AVL_DATA(node), njobs);

            job->tree.root = node;
            if ((job->job = vjob_run(avltree_visit_job, job)) == NULL) {
                ret = AVS_ERROR;
            }
            ++njobs;
        } else {
            /* keep parent of pushed nodes */
            avltree_visit_parallel_t * par = AVLTREE_JOB_BYIDX(data, nparents++, structsz);
            avltree_node_t * left = AVL_LEFT(node), * right = AVL_RIGHT(node);
            par->parent = node;

            if (left != NULL) {
                rbuf_push(fifo, left);
                rbuf_push(fifo, (void*)((unsigned long) (depth + 1)));
            }

            if (right != NULL) {
                rbuf_push(fifo, right);
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
    ctx->tree = tree;

    while (nparents-- > 0) {
        avltree_visit_parallel_t * par = AVLTREE_JOB_BYIDX(data, nparents, structsz);
        ctx->node = node = par->parent;
        //ctx->tree = &par->tree; // no using main tree.

        LOG_DEBUG(g_vlib_log, "avltree_parallel: parent visit - node %ld",
                  (long)AVL_DATA(node));

        if (visit(AVL_DATA(node), ctx, user_data) == AVS_ERROR)
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
            ctx->node = job->tree.root;
            if (visit(AVL_DATA(job->tree.root), ctx, user_data) == AVS_ERROR) {
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

static inline /*avltree_node_t * */void avltree_iterator_next_node(avltree_iterator_t * it, int ret, avltree_node_t * node, avltree_node_t * left, avltree_node_t * right) {
    /* prepare next visit */
    if (it->breadth_style) {
        /* push left/right child if required */
        if (it->push) {
            if ((node = avltree_visit_get_child(AGC_SECOND, ret, it, left,right)) != NULL) {
                rbuf_push(it->stack, node);
            }
            if ((node = avltree_visit_get_child(AGC_FIRST, ret, it, left,right)) != NULL) {
                rbuf_push(it->stack, node);
            }
            return ; //node;
        }
        return ; //it->destack(it->stack);
    }
    switch (it->context->state) {
        case AVH_PREFIX:
            /* push current node, so that child can return to its parent */
            if ((ret & AVS_SKIP) == 0) {
                rbuf_push(it->stack, node);
            }
            /* push first child if required (taking care of AVH_RIGHT modifier) */
            if (it->push
            &&  (node = avltree_visit_get_child(AGC_FIRST, ret, it, left, right)) != NULL) {
                /* visit first child in prefix mode */
                rbuf_push(it->stack, node);
            } else {
                if (!it->push && (it->how & AVH_INFIX) == 0) {
                    /* stay on node for suffix visit, as neither infix nor push is required */
                    it->context->state = AVH_SUFFIX;
                } else {
                    /* stay on node for infix visit */
                    it->context->state = AVH_INFIX;
                }
            }
            break ;
        case AVH_INFIX:
            /* push current node, so that child can return to its parent */
            if ((ret & AVS_SKIP) == 0) {
                rbuf_push(it->stack, node);
            }
            /* push left/right child if required (taking care of AVH_RIGHT modifier) */
            if (it->push
            &&  (node = avltree_visit_get_child(AGC_SECOND, ret, it, left, right)) != NULL) {
                /* go to right, for prefix visit */
                rbuf_push(it->stack, node);
                it->context->state = AVH_PREFIX;
            } else {
                /* stays on node, for SUFFIX visit */
                it->context->state = AVH_SUFFIX;
            }
            break ;
        case AVH_SUFFIX:
            if ((ret & AVS_NEXTVISIT) == AVS_NEXTVISIT) {
                break ;
            }
            if (rbuf_size(it->stack)) {
                avltree_node_t * parent = rbuf_top(it->stack);
                if ((it->push || (it->how & AVH_INFIX) != 0)
                && node == avltree_visit_get_child(AGC_FIRST, AVS_CONTINUE, it,
                                                   AVL_LEFT(parent), AVL_RIGHT(parent))) {
                    /* prepare the parent to be visited in infix mode */
                    it->context->state = AVH_INFIX;
                } else {
                    it->context->state = AVH_SUFFIX;
                }
                /* next visit is for parent */
            }
            break ;
        default:
            LOG_ERROR(g_vlib_log, "avltree_visit: bad state %d", it->context->state);
            ret = AVS_ERROR;
            rbuf_reset(it->stack);
            break ;
    }
    return ;//node;
}

/*****************************************************************************/
int                 avltree_visit(
                        avltree_t *                 tree,
                        avltree_visitfun_t          visit,
                        void *                      user_data,
                        avltree_visit_how_t         how) {
    avltree_iterator_t      it_fun, * it    = &it_fun;
    int                     ret             = AVS_CONTINUE;

    if (tree == NULL || visit == NULL) {
        return AVS_ERROR;
    }
    /* enable parallel only if more than 1 cpu and tree big enough */
    if ((how & AVH_PARALLEL) != 0 && (tree->flags & AFL_DISABLE_PARALLEL) == 0
    && (ret = vjob_cpu_nb()) > 1
    &&  tree->n_elements > 0 && 1 << (avltree_find_depth(tree) - 1) >= ret) {
        return avltree_visit_parallel(tree, visit, user_data, how);
    }
    /* iterator init including context / stack allocation if not shared or not available */
    if (avltree_iterator_init(it, tree, how) != AVS_FINISHED) {
        return AVS_ERROR;
    }

    while (rbuf_size(it->stack) != 0) {
        it->context->node = (avltree_node_t *) it->destack_fun(it->stack);

        //TODO: loop (stack mode) can be optimized by not pushing the next node
        //      to be visited, just do node = node->left/right
        //TODO: skip states which are not required by visitor (push child earlier)
        //TODO: AVS_NEXTVISIT and AVS_SKIP might not work correctly

        // It is ok to get left/right here, because visit_rebalance() is in suffix mode
        // and there is no left/right push in suffix mode. Moreover, getting left/right
        // now before visit is why the node free() operation works in prefix mode;
        avltree_node_t *      right = AVL_RIGHT(it->context->node);
        avltree_node_t *      left = AVL_LEFT(it->context->node);

        LOG_SCREAM(g_vlib_log, "altree_visit(): node:%ld(%ld,%ld) state:%d how:%d "
                               "do_visit:%d last_ret:%d how_orig:%d",
                (long) AVL_DATA(it->context->node),
                left  ? (long)AVL_DATA(left)  : -1,
                right ? (long)AVL_DATA(right) : -1,
                it->context->state, it->how, (it->how &~AVH_RIGHT & it->context->state),
                ret, it->context->how);

        /* visit the current node if required, and update ret only if visitor is called */
        if ((it->how & ~AVH_RIGHT & it->context->state) == it->context->state) {

            /* call the visit() function */
            ret = visit(AVL_DATA(it->context->node), it->context, user_data);

            /* stop on error or when visit goal is accomplished */
            if (ret == AVS_ERROR || ret == AVS_FINISHED) {
                break ;
            }
            /* we remove the current visit type from set */
            if ((ret & AVS_NEXTVISIT) == AVS_NEXTVISIT) {
                it->how &= ~(it->context->state);
                it->push = 0;
            }
        }

        //node =
        avltree_iterator_next_node(it, ret, it->context->node, left, right);

    }
    /* free resources if needed, and restore shared resources */
    //avltree_visit_resources_free(tree, it->stack, it->context);
    avltree_iterator_clean(it);

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

// ***************************************************************************
static inline int   avltree_iterator_init(
                        avltree_iterator_t *        it,
                        avltree_t *                 tree,
                        avltree_visit_how_t         how) {

    if (avltree_visit_resources_get(tree, &(it->stack), &(it->context)) != AVS_FINISHED) {
        return AVS_ERROR;
    }
    it->context->tree = tree;

    it->allocated = 0;
    it->breadth_style = 0;
    it->push = -1;
    it->invert_childs = (((how & (AVH_BREADTH | AVH_RIGHT)) == AVH_BREADTH)
                      || ((how & (AVH_RIGHT | AVH_BREADTH)) == AVH_RIGHT));
    it->how = it->context->how = how;
    it->destack_fun = (it->how & AVH_BREADTH) != 0 ? rbuf_dequeue : rbuf_pop;

    /* push the root node, if any */
    if (tree->root != NULL) {
        rbuf_push(it->stack, tree->root);
    }

    it->context->level = 0;
    it->context->index = 0;
    it->context->stack = it->stack;
    it->context->data = NULL;

    it->how &= (AVH_MASK & ~(AVH_PARALLEL) & ~(AVH_MERGE));
    it->context->state = (it->how & AVH_BREADTH) != 0 ? (it->how & ~AVH_RIGHT & AVH_BREADTH) : AVH_PREFIX;
    it->breadth_style = (it->context->state == AVH_BREADTH || (it->how & ~AVH_RIGHT) == AVH_PREFIX);

    return AVS_FINISHED;
}

// ***************************************************************************
static inline void  avltree_iterator_clean(avltree_iterator_t * iterator) {
    avltree_visit_resources_free(iterator->context->tree, iterator->stack, iterator->context);
}

// ***************************************************************************
avltree_iterator_t* avltree_iterator_create(
                        avltree_t *                 tree,
                        avltree_visit_how_t         how) {
    avltree_iterator_t * iterator;

    if (tree == NULL || (how & AVH_PARALLEL) != 0) {
        errno = EINVAL;
        return NULL;
    }
    if ((iterator = malloc(sizeof(*iterator))) == NULL) {
        return NULL;
    }
    if (avltree_iterator_init(iterator, tree, how) != AVS_FINISHED) {
        free(iterator);
        return NULL;
    }
    iterator->allocated = -1;

    LOG_DEBUG(g_vlib_log, "%s(): done (tree:%lx, it:%lx).",
                  __func__, (unsigned long) iterator->context->tree, (unsigned long) iterator);

    return iterator;
}

// ***************************************************************************
void *              avltree_iterator_next(avltree_iterator_t * it) {
    avltree_node_t *    visit_node;
    int                 ret = AVS_CONTINUE;

    while (rbuf_size(it->stack) != 0) {
        it->context->node = (avltree_node_t *) it->destack_fun(it->stack);
        visit_node = NULL;

        // It is ok to get left/right here, because visit_rebalance() is in suffix mode
        // and there is no left/right push in suffix mode. Moreover, getting left/right
        // now before visit is why the node free() operation works in prefix mode;
        avltree_node_t *      right = AVL_RIGHT(it->context->node);
        avltree_node_t *      left = AVL_LEFT(it->context->node);

        LOG_DEBUG(g_vlib_log, "altree_iterator_next(): node:%ld(%ld,%ld) state:%d how:%d "
                               "do_visit:%d last_ret:%d how_orig:%d",
                (long) AVL_DATA(it->context->node),
                left?  (long)AVL_DATA(left):  -1,
                right? (long)AVL_DATA(right): -1,
                it->context->state, it->how, (it->how &~AVH_RIGHT & it->context->state), ret, it->context->how);

        /* visit the current node if required, and update ret only if visitor is called */
        if ((it->how & ~AVH_RIGHT & it->context->state) == it->context->state) {
            visit_node = it->context->node;
        }
        //it->context->node =
        avltree_iterator_next_node(it, ret, it->context->node, left, right);

        if (visit_node) {
            void * data = AVL_DATA(visit_node);
            if (data == NULL) {
                errno = 0;
            }
            return data;
        }
    }

    avltree_iterator_abort(it);
    errno = ENOENT;
    return NULL;
}

// ***************************************************************************
avltree_visit_context_t * avltree_iterator_context(avltree_iterator_t * iterator) {
    if (iterator == NULL) {
        errno = EINVAL;
        return NULL;
    }
    return iterator->context;
}

// ***************************************************************************
void                avltree_iterator_abort(avltree_iterator_t * iterator) {
    if (iterator != NULL) {
        LOG_DEBUG(g_vlib_log, "%s(): aborting (tree:%lx, it:%lx)",
                  __func__, (unsigned long) iterator->context->tree, (unsigned long) iterator);

        avltree_iterator_clean(iterator);
        if (iterator->allocated)
            free(iterator);
    }
}

/*****************************************************************************/
typedef struct {
    slist_t * head;
    slist_t * tail;
} avltree_slist_data_t;

static AVLTREE_DECLARE_VISITFUN(avltree_toslist_visit, node_data, context, vdata) {
    avltree_slist_data_t *  data = (avltree_slist_data_t *) vdata;

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
        slist_t * new = slist_prepend(NULL, node_data);
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
static AVLTREE_DECLARE_VISITFUN(avltree_torbuf_visit, node_data, context, vdata) {
    rbuf_t ** prbuf = (rbuf_t **) vdata;

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
        && NULL == (*prbuf = rbuf_create((context->tree->n_elements / vjob_cpu_nb()) + 1, RBF_DEFAULT))) {
            return AVS_ERROR;
        }
        if (rbuf_push(*prbuf, node_data) != 0) {
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
static AVLTREE_DECLARE_VISITFUN(avltree_toarray_visit, node_data, context, vdata) {
    avltree_array_visit_data_t * data = (avltree_array_visit_data_t *) vdata;
    (void) context;
    if (data->n >= data->max)
        return AVS_ERROR;
    data->array[(data->n)++] = node_data;
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

static AVLTREE_DECLARE_VISITFUN(avltree_visit_rebalance, node_data, context, user_data) {
    avltree_t *                 tree        = context->tree;
    avltree_node_t *            node        = context->node;
    avltree_visit_insert_t *    idata       = (avltree_visit_insert_t *) user_data;
    avltree_node_t *            parent      = rbuf_top(context->stack); /* suffix mode, top=parent */
    avltree_node_t **           pparent;
    int                         balance_dir = AVL_LEFT_PTR(node) == idata->prev_nodep ? -1 : +1;
    (void) node_data;

    pparent = parent ? (AVL_LEFT(parent) == node ? AVL_LEFT_PTR(parent) : AVL_RIGHT_PTR(parent)) : &tree->root;

    LOG_SCREAM(g_vlib_log, "BALANCING %ld(%ld,%ld) ppar=%ld "
                           "difbal:%d dirbal:%d obal:%d nbal:%d (%s:%ld)",
                (long) AVL_DATA(node),
                AVL_LEFT(node) ? (long) AVL_DATA(AVL_LEFT(node)) : -1,
                AVL_RIGHT(node) ? (long) AVL_DATA(AVL_RIGHT(node)) : -1,
                pparent ? (long)AVL_DATA(AVL_NODE(pparent)) : -1,
                idata->new_balance, balance_dir,
                AVL_BALANCE(node),
                AVL_BALANCE(node) + (balance_dir * idata->new_balance),
                idata->new_balance < 0 ? "Deleted" : "Inserted",
                (long) idata->newdata);

    int old_balance = AVL_BALANCE(node), new_balance = old_balance;
    if (idata->new_balance >= 0) {
        new_balance += (balance_dir * idata->new_balance);
        if (balance_dir == -1 && old_balance <= 0) {
            /* left subtree increases */
        } else if (balance_dir == 1 && old_balance >= 0) {
            /* right subtree increases */
        } else {
            AVL_SET_BALANCE(node, new_balance);
            return AVS_FINISHED;
        }
    } else if (idata->new_balance < 0) {
        new_balance += (balance_dir * idata->new_balance);
        if (balance_dir == -1 && old_balance > 0) {
            /* left subtree decreases */
        } else if (balance_dir == 1 && old_balance < 0) {
            /* right subtree decreases */
        } else if (old_balance == 0) {
            AVL_SET_BALANCE(node, new_balance);
            return AVS_FINISHED;
        }
    }

    // If a rotation is performed, the AVL_BALANCE(node) will be overwritten,
    // then no need to do AVL_SET_BALANCE(new_balance) except if no rotation is done.
    if (new_balance < -1) {
        _DEBUG_PRINT_TREE(tree);
        /* right rotate */
        if ((old_balance = AVL_BALANCE(AVL_LEFT(node))) > 0) {
            //double rotate
            LOG_DEBUG(g_vlib_log, "left(%ld) right(%ld) rotation",
                                  (long)AVL_DATA(AVL_LEFT(node)), (long) AVL_DATA(node));
            AVL_SET_NODE(pparent, avltree_rotate_leftright(tree, node));
        } else {
            LOG_DEBUG(g_vlib_log, "right(%ld) rotation", (long) AVL_DATA(node));
            AVL_SET_NODE(pparent, avltree_rotate_right(tree, node, 1));
        }
        idata->prev_nodep = pparent;
        if (idata->new_balance >= 0 || old_balance == 0)
            return AVS_FINISHED;
    } else if (new_balance > 1) {
        _DEBUG_PRINT_TREE(tree);
        // left rotate
        if ((old_balance = AVL_BALANCE(AVL_RIGHT(node))) < 0) {
            //double rotation
            LOG_DEBUG(g_vlib_log, "right(%ld) left(%ld) rotation",
                                  (long)AVL_DATA(AVL_RIGHT(node)), (long) AVL_DATA(node));
            AVL_SET_NODE(pparent, avltree_rotate_rightleft(tree, node));
        } else {
            LOG_DEBUG(g_vlib_log, "left(%ld) rotation", (long) AVL_DATA(node));
            AVL_SET_NODE(pparent, avltree_rotate_left(tree, node, 1));
        }
        idata->prev_nodep = pparent;
        if (idata->new_balance >= 0 || old_balance == 0)
            return AVS_FINISHED;
    } else {
        AVL_SET_BALANCE(node, new_balance);
    }

    idata->prev_nodep = pparent;

    return AVS_CONTINUE;
}

/*****************************************************************************/
static AVLTREE_DECLARE_VISITFUN(avltree_visit_insert, node_data, context, user_data) {
    avltree_t *                 tree = context->tree;
    avltree_node_t *            node = context->node;
    avltree_visit_insert_t *    idata = (avltree_visit_insert_t *) user_data;
    avltree_node_t *            new;
    avltree_node_t **           parent;
    int                         cmp;

    if (context->state == AVH_SUFFIX) {
        return avltree_visit_rebalance(node_data, context, user_data);
    } else if (context->state != AVH_PREFIX) {
        return AVS_ERROR;
    }

    LOG_SCREAM(g_vlib_log, "INSERTING %ld, Visiting Node %ld(%ld,%ld)"
                           " State:%d How:%d Ptr %lx(%lx,%lx)",
              (long)idata->newdata, (long)AVL_DATA(node),
              AVL_LEFT(node)?(long)AVL_DATA(AVL_LEFT(node)):-1, AVL_RIGHT(node)?(long)AVL_DATA(AVL_RIGHT(node)):-1,
              context->state, context->how,
              (unsigned long) node, (unsigned long)AVL_LEFT(node), (unsigned long)AVL_RIGHT(node));

    cmp = tree->cmp(idata->newdata, node_data);
    if (cmp == 0 && (cmp = (tree->flags & AFL_INSERT_MASK)) != 0) {
        if (cmp == AFL_INSERT_NODOUBLE) {
            /* having doubles is forbidden in this tree: return error */
            LOG_DEBUG(g_vlib_log, "%s(): %lx already in tree, keep it - "
                                  "node %ld(%lx,%lx) ptr 0x%lx", __func__,
              (long)(idata->newdata),
              (long)node_data, (unsigned long)AVL_LEFT(node), (unsigned long)AVL_RIGHT(node),
              (unsigned long) node);

            return AVS_ERROR;
        }
        /* replace or ignore exsting element: previous one is returned */
        void * prevdata = node_data;
        if (cmp == AFL_INSERT_REPLACE) {
            /* replace data in the node, return previous data, and free it if needed */
            LOG_DEBUG(g_vlib_log, "%s(): %lx already in tree, replace it - "
                                  "node %ld(%lx,%lx) ptr 0x%lx", __func__,
              (long)(idata->newdata),
              (long)node_data, (unsigned long)AVL_LEFT(node), (unsigned long)AVL_RIGHT(node),
              (unsigned long) node);

            AVL_SET_DATA(node, idata->newdata);
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
        if (AVL_LEFT(node) != NULL) {
            /* continue with left node */
            return AVS_GO_LEFT;
        }
        /* left is null, we insert on node->left */
        parent = AVL_LEFT_PTR(node);
    } else {
        /* go right */
        if (AVL_RIGHT(node) != NULL) {
            /* continue with right node */
            return AVS_GO_RIGHT;
        }
        /* right is null, we insert on node->right */
        parent = AVL_RIGHT_PTR(node);
    }
    /* Node Creation */
    new = avltree_node_alloc(tree);
    if (new == NULL) {
        return AVS_ERROR;
    }
    AVL_SET_BALANCE(new, 0);
    AVL_SET_LEFT(new, NULL);
    AVL_SET_RIGHT(new, NULL);
    AVL_SET_DATA(new, idata->newdata);
    AVL_SET_NODE(parent, new);
    idata->newnode = new;
    idata->prev_nodep = parent;
    idata->new_balance = 1;

    LOG_DEBUG(g_vlib_log, "%s(): INSERTED node %ld(%lx,%lx) ptr 0x%lx on %s of %ld ptr 0x%lx",
              __func__,
              (long)AVL_DATA(new), (unsigned long)AVL_LEFT(new), (unsigned long)AVL_RIGHT(new),
              (unsigned long) new,
              parent == AVL_LEFT_PTR(node) ? "LEFT" : "RIGHT",
              (long)AVL_DATA(node), (unsigned long) node);

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
static AVLTREE_DECLARE_VISITFUN(avltree_visit_remove, node_data, context, user_data) {
    avltree_t *                 tree = context->tree;
    avltree_node_t *            node = context->node, *left, *right;
    avltree_visit_insert_t *    idata = (avltree_visit_insert_t *) user_data;
    avltree_node_t **           parent;
    int                         cmp;

    if (context->state == AVH_SUFFIX) {
        return avltree_visit_rebalance(node_data, context, user_data);
    } else if (context->state != AVH_PREFIX) {
        return AVS_ERROR;
    }
    left = AVL_LEFT(node);
    right = AVL_RIGHT(node);

    LOG_SCREAM(g_vlib_log, "DELETING %ld, Visiting Node %ld(%ld,%ld)"
                           " State:%d How:%d Ptr %lx(%lx,%lx)",
              (long)idata->newdata, (long)node_data,
              left?(long)AVL_DATA(left):-1, right?(long)AVL_DATA(right):-1,
              context->state, context->how,
              (unsigned long) node, (unsigned long)left, (unsigned long)right);

    /* keep the parent of node : works because we are in prefix mode */
    parent = idata->prev_nodep;

    /* compare node value with searched one */
    if ((cmp = tree->cmp(idata->newdata, node_data)) < 0) {
        /* continue with left node, or raise error if NULL */
        if (left == NULL) {
            return AVS_ERROR;
        }
        idata->prev_nodep = AVL_LEFT_PTR(node);
        return AVS_GO_LEFT;
    } else if (cmp > 0) {
        /* continue with right node, or raise error if NULL */
        if (right == NULL) {
            return AVS_ERROR;
        }
        idata->prev_nodep = AVL_RIGHT_PTR(node);
        return AVS_GO_RIGHT;
    }
    /* Found. Node Deletion.
     * When D is root, make R root : done with AVL_SET_NODE(parent, ...),
     *   (prev_nodep set to root in avltree_remove). */
    idata->newdata = node_data;
    if (left == NULL && right == NULL) {
        /* 1) deleting a node with no children: remove the node */
        LOG_DEBUG(g_vlib_log, "avltree_remove(%ld): case 1 (no child)", (long)(idata->newdata));
        AVL_SET_NODE(parent, NULL);
        avltree_node_free(tree, node, (tree->flags & AFL_REMOVE_NOFREE) == 0);
        idata->newnode = node;
        idata->prev_nodep = parent;
    } else if (left == NULL || right == NULL) {
        /* 2) deleting a node with one child: remove the node and replace it with its child */
        LOG_DEBUG(g_vlib_log, "avltree_remove(%ld): case 2 (1 child)", (long)(idata->newdata));
        AVL_SET_NODE(parent, (left != NULL ? left : right));
        avltree_node_free(tree, node, (tree->flags & AFL_REMOVE_NOFREE) == 0);
        idata->newnode = node;
        idata->prev_nodep = parent;
    } else {
        /* 3) deleting a node(N) with 2 children: do not delete N:
         *    choose infix successor as replacement (R), copy value of R to N.*/
        rbuf_push(context->stack, node);
        avltree_node_t ** preplace = AVL_RIGHT_PTR(node), * replace;
        while (AVL_LEFT((replace = AVL_NODE(preplace))) != NULL) {
            rbuf_push(context->stack, replace);
            preplace = AVL_LEFT_PTR(replace);
        }
        idata->prev_nodep = preplace;
        LOG_DEBUG(g_vlib_log, "avltree_remove(%ld): case 3%c (2 children), replace:%ld(%ld:%ld)",
                  (long)AVL_DATA(node), AVL_RIGHT(replace) == NULL ? 'a' : 'b',
                  (long)AVL_DATA(replace),
                  AVL_LEFT(replace) ? (long) AVL_DATA(AVL_LEFT(replace)) : -1l,
                  AVL_RIGHT(replace) ? (long) AVL_DATA(AVL_RIGHT(replace)) : -1l);

        /* a&b) if R has a child (C,right child), replace R with C at R's parent, else remove it. */
        AVL_SET_DATA(node, AVL_DATA(replace));
        AVL_SET_DATA(replace, idata->newdata);
        AVL_SET_NODE(preplace, AVL_RIGHT(replace));
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
static AVLTREE_DECLARE_VISITFUN(avltree_visit_free, node_data, context, user_data) {
    (void) node_data;
    (void) user_data;

    avltree_node_free(context->tree, context->node, 1);
    return AVS_CONTINUE;
}

/*****************************************************************************/
static AVLTREE_DECLARE_VISITFUN(avltree_visit_find_range, node_data, context, user_data) {
    avltree_t *             tree = context->tree;
    //avltree_node_t *        node = context->node;
    avltree_visit_range_t * rangedata = (avltree_visit_range_t *) user_data;

    if (context->state == AVH_PREFIX) {
        if (tree->cmp(node_data, rangedata->min) < 0) {
            return AVS_GO_RIGHT;
        }
        return AVS_GO_LEFT;
    } else if (context->state == AVH_INFIX) {
        if (tree->cmp(node_data, rangedata->min) < 0) {
            return AVS_CONTINUE;
        }
        if (tree->cmp(node_data, rangedata->max) > 0) {
            return AVS_FINISHED;
        }
        if (rangedata->visit == NULL) {
            return AVS_CONTINUE;
        }
        return rangedata->visit(node_data, context, rangedata->user_data);
    }
    return AVS_ERROR;
}

/*****************************************************************************/
static avltree_node_t * avltree_rotate_left(avltree_t * tree, avltree_node_t * node, int update) {
    (void) tree;
    avltree_node_t * rchild = AVL_RIGHT(node);

    AVL_SET_RIGHT(node, AVL_LEFT(rchild));
    AVL_SET_LEFT(rchild, node);

    if (update == 1) {
        if (AVL_BALANCE(rchild) == 0) {
            AVL_SET_BALANCE(node, 1);
            AVL_SET_BALANCE(rchild, -1);
        } else {
            AVL_SET_BALANCE(node, 0);
            AVL_SET_BALANCE(rchild, 0);
        }
    }
    return rchild;
}
/*****************************************************************************/
static avltree_node_t * avltree_rotate_right(avltree_t * tree, avltree_node_t * node, int update) {
    (void) tree;
    avltree_node_t * lchild = AVL_LEFT(node);

    AVL_SET_LEFT(node, AVL_RIGHT(lchild));
    AVL_SET_RIGHT(lchild, node);

    if (update == 1) {
        if (AVL_BALANCE(lchild) == 0) {
            AVL_SET_BALANCE(node, -1);
            AVL_SET_BALANCE(lchild, 1);
        } else {
            AVL_SET_BALANCE(node, 0);
            AVL_SET_BALANCE(lchild, 0);
        }
    }
    return lchild;
}
/*****************************************************************************/
static avltree_node_t * avltree_rotate_rightleft(avltree_t * tree, avltree_node_t * node) {
    (void) tree;
    avltree_node_t * rchild = AVL_RIGHT(node);
    avltree_node_t * l_of_rchild = AVL_LEFT(rchild);
    avltree_node_t * newroot;

    AVL_SET_RIGHT(node, avltree_rotate_right(tree, AVL_RIGHT(node), 0));
    newroot = avltree_rotate_left(tree, node, 0);

    if (AVL_BALANCE(l_of_rchild) > 0) {
        AVL_SET_BALANCE(node, -1);
        AVL_SET_BALANCE(rchild, 0);
    } else if (AVL_BALANCE(l_of_rchild) == 0) {
        AVL_SET_BALANCE(node, 0);
        AVL_SET_BALANCE(rchild, 0);
    } else {
        AVL_SET_BALANCE(node, 0);
        AVL_SET_BALANCE(rchild, 1);
    }
    AVL_SET_BALANCE(l_of_rchild, 0);

    return newroot;
}
/*****************************************************************************/
static avltree_node_t * avltree_rotate_leftright(avltree_t * tree, avltree_node_t * node) {
    (void) tree;
    avltree_node_t * lchild = AVL_LEFT(node);
    avltree_node_t * r_of_lchild = AVL_RIGHT(lchild);
    avltree_node_t * newroot;

    AVL_SET_LEFT(node, avltree_rotate_left(tree, AVL_LEFT(node), 0));
    newroot = avltree_rotate_right(tree, node, 0);

    if (AVL_BALANCE(r_of_lchild) < 0) {
        AVL_SET_BALANCE(node, 1);
        AVL_SET_BALANCE(lchild, 0);
    } else if (AVL_BALANCE(r_of_lchild) == 0) {
        AVL_SET_BALANCE(node, 0);
        AVL_SET_BALANCE(lchild, 0);
    } else {
        AVL_SET_BALANCE(node, 0);
        AVL_SET_BALANCE(lchild, -1);
    }
    AVL_SET_BALANCE(r_of_lchild, 0);

    return newroot;
}

/*****************************************************************************/
int                 avltree_print_node_default(
                        FILE *                      out,
                        const avltree_node_t *      node) {
    return fprintf(out, "%ld(%d)", (long)AVL_DATA(node), AVL_BALANCE(node));
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
        avltree_node_t    * left    = AVL_LEFT(node), * right = AVL_RIGHT(node);

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

        if (left) {
            rbuf_push(fifo, (void*)((long)(level + 1)));
            rbuf_push(fifo, (void*)((ssize_t)(idx * 2)));
            rbuf_push(fifo, left);
        }
        if (right) {
            rbuf_push(fifo, (void*)((long)(level + 1)));
            rbuf_push(fifo, (void*)((ssize_t)(idx * 2 + 1)));
            rbuf_push(fifo, right);
        }
    }
    fputc('\n', out);

    avltree_visit_resources_free(tree, fifo, context);
}

