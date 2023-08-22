/*
 * Copyright (C) 2018-2020,2023 Vincent Sallaberry
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
#ifndef VLIB_AVLTREE_H
#define VLIB_AVLTREE_H

#include <stdio.h>
#include <pthread.h>

#include "vlib/rbuf.h"
#include "vlib/slist.h"

/** avltree_node_t data compare function, strcmp-like */
typedef int     (*avltree_cmpfun_t)   (const void *, const void *);
/** avltree_node_t data free function, free-like */
typedef void    (*avltree_freefun_t)  (void *);

/** opaque struct avltree_node_t */
typedef struct avltree_node_s           avltree_node_t;

/** avltree_visit_context_t defined later */
typedef struct avltree_visit_context_s  avltree_visit_context_t;

/** tree flags */
typedef enum {
    AFL_NONE            = 0,
    /** AFL_SHARED_STACK: Request to create internal stack and context, shared
     * between the tree operations. Otherwise, if the tree stack or context
     * (tree->{stack,context}) is NULL, tree operations will create and free
     * a stack and context at each call. If not NULL, tree operations will use
     * the stack and context even without AFL_SHARED_STACK, allowing sharing a
     * stack and context between several trees. */
    AFL_SHARED_STACK    = 1 << 0,
    AFL_REMOVE_NOFREE   = 1 << 1,   /* don't call tree->free() on remove, caller must free data */
    AFL_INSERT_NODOUBLE = 1 << 2,   /* return error when inserting existing element (cmp==0) */
    AFL_INSERT_IGNDOUBLE= 1 << 3,   /* ignore insertion of existing element (cmp==0) */
    /* replace an already existing element on insert (cmp==0) */
    AFL_INSERT_REPLACE  = AFL_INSERT_NODOUBLE | AFL_INSERT_IGNDOUBLE,
    AFL_INSERT_MASK     = AFL_INSERT_REPLACE,
    AFL_FREE_PARALLEL   = 1 << 4,   /* perform multi-threaded avltree_{free,clear}() */
    AFL_DISABLE_PARALLEL= 1 << 5,   /* forbids completly parallel visits */
    AFL_USER            = 1 << 16,
    AFL_DEFAULT         = AFL_SHARED_STACK | AFL_FREE_PARALLEL
} avltree_flags_t;

/** avltree_t */
typedef struct {
    avltree_node_t *            root;
    size_t                      n_elements;
    avltree_flags_t             flags;
    avltree_cmpfun_t            cmp;
    avltree_freefun_t           free;
    struct avltree_shared_s *   shared;
} avltree_t;

/** resources shared between several trees */
typedef struct avltree_shared_s {
    struct rbuf_s *             stack;
    avltree_visit_context_t *   context;
    pthread_mutex_t             mutex;
    int                         in_use;
} avltree_shared_t;

/** avltree_visitfun_t return value */
typedef enum {
    AVS_GO_LEFT     = 1 << 0,   /* only go to left node, but continue visit */
    AVS_GO_RIGHT    = 1 << 1,   /* only go to right node, but continue visit */
    AVS_SKIP        = 1 << 2,   /* skip this node, but continue visit */
    AVS_NEXTVISIT   = 1 << 3,   /* stop current visit, stop pushing childs, and start next visit */
    AVS_CONTINUE    = AVS_GO_LEFT | AVS_GO_RIGHT, /* continue visit */
    AVS_ERROR       = -1,       /* stop visit, report error */
    AVS_FINISHED    = 0         /* stop visit, report success */
} avltree_visit_status_t;

/** how to visit the tree (direction) */
#define AVH_PARALLEL_SHIFT (16U)
#define AVH_PARALLEL_DUPDATA(_how, _datasz) \
        (((_how) & AVH_MASK) | AVH_PARALLEL \
         | ((unsigned int) (_datasz) << AVH_PARALLEL_SHIFT))
#define AVH_PARALLEL_DATASZ(_how) \
        (((_how) & AVH_PARALLEL) == 0 \
         ? 0U : (unsigned int)((unsigned int)(_how) >> AVH_PARALLEL_SHIFT))
typedef enum {
    AVH_PREFIX  = 1 << 0,   /* prefix, pre-order (first visit, before the two childs) */
    AVH_INFIX   = 1 << 1,   /* infix, in-order (second visit between the two childs) */
    AVH_SUFFIX  = 1 << 2,   /* suffix, post-order (third visit, after the two childs) */
    AVH_BREADTH = 1 << 3,   /* breadth-first (width visit) */
    AVH_PARALLEL= 1 << 5,   /* visit modifier: multi-threaded visit, undefined order
                            With AVH_PARALLEL_DUPDATA, user_data is duplicated for each job */
    AVH_MERGE   = 1 << 6,   /* visit modifier: with AVH_PARALLEL, merge by visiting roots
                            of jobs subtrees with AVH_MERGE and context.data=job_user_data */
    AVH_RIGHT   = 1 << 7,   /* visit modifier: visit right child before left child */
    AVH_MASK   = ((1U << AVH_PARALLEL_SHIFT) - 1U),
} avltree_visit_how_t;

/** visit context to be passed to avltree_visitfun_t functions */
struct avltree_visit_context_s {
    avltree_t *                 tree;   /* the tree being visited */
    avltree_visit_how_t         state;  /* current visit state (prefix,infix,...) */
    avltree_visit_how_t         how;    /* requested visit modes (prefix|infix|...) */
    avltree_node_t *            node;   /* the node being visited, cannot be NULL (ensured by avltree_visit()) */
    size_t                      level;  /* current node level (depth) */
    size_t                      index;  /* current node index in level */
    rbuf_t *                    stack;  /* current stack */
    void *                      data; /* RFU, used for PARALLEL MERGE */
};

/** avltree_node_t visit function called on each node by avltree_visit()
 * @param data, the data within the node beiing visited,
 * @param context avltree_visit_context_t struct with node infos and visit state,
 *        cannot be NULL (ensured by avltree_visit())
 * @param user_data the specific user data given to avltree_visit
 * @return avltree_visit_status_t
 */
typedef int         (*avltree_visitfun_t) (
                        void *                              data,
                        const avltree_visit_context_t *     context,
                        void *                              user_data);
#define AVLTREE_DECLARE_VISITFUN(_name, _data, _context, _user_data) \
    avltree_visit_status_t _name(                                           \
                            void *                          _data,          \
                            const avltree_visit_context_t * _context,       \
                            void *                          _user_data)

/** AVLNODE : shortcut for avltree_node_create() */
#define AVLNODE(value, left, right)     avltree_node_create(NULL, value, left, right)

/*****************************************************************************/

/* General information about inserted data and functions returned values:
 * As NULL values can be inserted in avltree, some get functions can return NULL
 * on success. When the returned value is NULL, errno is set to 0 in the function,
 * and when an error occurs, NULL is returned with errno != 0.
 * When user inserts NULL values, he must test errno as below:
 * 'if (avltree_find(tree, data) == NULL && errno != 0) perror("find");' */

/*****************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/***********************
 * AVL NODE OPERATIONS *
 * *********************/

/** get the size of an avltree_node_t, not including what could be in data address. */
size_t              avltree_node_size();
/** get left child of an avltree_node_t */
avltree_node_t *    avltree_node_left(const avltree_node_t * node);
/** get right child of an avltree_node_t */
avltree_node_t *    avltree_node_right(const avltree_node_t * node);
/** get balance of an avltree_node_t */
char                avltree_node_balance(const avltree_node_t * node);
/** get data of an avltree_node_t */
void *              avltree_node_data(const avltree_node_t * node);
/** set data of an avltree_node_t */
void                avltree_node_set_data(avltree_node_t * node, void * data);


/***********************
 * AVL TREE OPERATIONS *
 * *********************/

/** avltree_create()
 * complexity: O(1) */
avltree_t *         avltree_create(
                        avltree_flags_t             flags,
                        avltree_cmpfun_t            cmpfun,
                        avltree_freefun_t           freefun);

/** avltree_node_create
 * complexity: O(1) */
avltree_node_t *    avltree_node_create(
                        avltree_t *                 tree,
                        void *                      data,
                        avltree_node_t *            left,
                        avltree_node_t *            right);

/** avltree_insert()
 * complexity: O(log2(n))
 * @return inserted element if not replaced/ignored,
 *         or previous element if it has been replaced (AFL_INSERT_REPLACE ON),
 *            previous element is freed with tree->free if AFL_REMOVE_NOFREE is OFF.
 *         or previous element if AFL_INSERT_IGNDOUBLE is ON,
 *         or NULL on error with errno set.
 *         (on success, errno is only set to 0 if element is NULL). */
void *              avltree_insert(
                        avltree_t *                 tree,
                        void *                      data);

/** avltree_clear()
 * complexity: O(n)
 * @return avltree_visit_status_t */
int                avltree_clear(
                        avltree_t *                 tree);

/** avltree_free()
 * complexity: O(n) */
void                avltree_free(
                        avltree_t *                 tree);

/** avltree_find().
 * complexity: O(log2(n))
 * @return the element (errno is only set to 0 if element is NULL),
 *         or NULL on error with errno set. */
void *              avltree_find(
                        avltree_t *                 tree,
                        const void *                data);

/** avltree_find_min()
 * complexity: O(log2(n))
 * @return the min element (errno is only set to 0 if element is NULL),
 *         or NULL on error with errno set. */
void *              avltree_find_min(
                        avltree_t *                 tree);

/** avltree_find_max()
 * complexity: O(log2(n))
 * @return the max element (errno is only set to 0 if element is NULL),
 *         or NULL on error with errno set. */
void *              avltree_find_max(
                        avltree_t *                 tree);

/** avltree_find_depth()
 * complexity: O(log2(n))
 * @return the depth of tree (errno is only set to 0 if depth is 0),
 *             or 0 on error with errno set. */
unsigned int        avltree_find_depth(
                        avltree_t *                 tree);

/** avltree_count()
 * complexity: O(1)
 * @return number of elements in the tree (errno is only set to 0 if count is 0),
 *         or 0 on error with errno set. */
size_t              avltree_count(
                        avltree_t *                 tree);

/** avltree_memorysize()
 * complexity: O(1)
 * @return estimation of memory used by the tree (except size of nodes datas,
 *           errno is only set to 0 if size is 0),
 *         or 0 on error with errno set. */
size_t              avltree_memorysize(
                        avltree_t *                 tree);

/** avltree_visit()
 * The given function will be called on each node in an order specified by how.
 * @param tree the tree to visit
 * @param visit the visit function (avltree_visitfun_t)
 * @param user_data the specific user_data to give to visit function
 * @param how a combination of avltree_visit_how_t, giving visit type(s) (prefix|infix|...)
 * @return avltree_visit_status_t */
int                 avltree_visit(
                        avltree_t *                 tree,
                        avltree_visitfun_t          visit,
                        void *                      user_data,
                        avltree_visit_how_t         how);

/** avltree_visit_range()
 * The given function will be called in infix order on each node of given range.
 * Parallel mode (AVH_PARALLEL) is not supported and ignored (normat infix done).
 * @param tree the tree to visit
 * @param min the minimum element of the range
 * @param max the maximum element of the range
 * @param visit the visit function (avltree_visitfun_t)
 * @param user_data the specific user_data to give to visit function
 * @param how_RFU RFU
 * @return avltree_visit_status_t */
int                 avltree_visit_range(
                        avltree_t *                 tree,
                        void *                      min,
                        void *                      max,
                        avltree_visitfun_t          visit,
                        void *                      user_data,
                        avltree_visit_how_t         how_RFU);

/** avltree_remove()
 * complexity: O(log2(n))
 * @return the removed element, (errno is only set to 0 if element is NULL).
 *          ! Warning: depending on the avltree_freefun_t function and the flag
 *          ! AFL_REMOVE_NOFREE given in avltree_create, returned value might be freed.
 *         or NULL on error with errno set. */
void *              avltree_remove(
                        avltree_t *                 tree,
                        const void *                data);

/** avltree_to_slist()
 * complexity: O(n)
 * return a single-linked list of tree elements (node->data), sorted according to 'how',
 * unless AVH_PARALLEL is used (random order).
 * @param tree the tree to visit
 * @param how a combination of avltree_visit_how_t, giving list order (prefix|infix|...)
 * @return the list (free with slist_free(list, NULL) or NULL or NULL on error with errno set.
 * @notes: utility function only, lot of mallocs, avltree_visit better for frequent use */
slist_t *           avltree_to_slist(
                        avltree_t *                 tree,
                        avltree_visit_how_t         how);

/** avltree_to_rbuf()
 * complexity: O(n)
 * return a ring-buffer filled with tree elements (node->data), sorted according to 'how',
 * unless AVH_PARALLEL is used (random order).
 * @param tree the tree to visit
 * @param how a combination of avltree_visit_how_t, giving rbuf order (prefix|infix|...)
 * @return the rbuf (free with rbuf_free(rbuf)) or NULL or NULL on error with errno set.
 * @notes: utility function only, lot of mallocs, avltree_visit better for frequent use */
rbuf_t *            avltree_to_rbuf(
                        avltree_t *                 tree,
                        avltree_visit_how_t         how);

/** avltree_to_array()
 * complexity: O(n)
 * Creates an array filled with tree elements (node->data), sorted according to 'how',
 * unless AVH_PARALLEL is used (random order).
 * @param tree the tree to visit
 * @param how a combination of avltree_visit_how_t, giving array order (prefix|infix|...)
 * @param parray pointer to array of <void *> (free with free(*parray) if not NULL).
 * @return the number of element in array or 0 on error with errno set.
 * @notes: utility function only, lot of mallocs, avltree_visit better for frequent use */
size_t              avltree_to_array(
                        avltree_t *                 tree,
                        avltree_visit_how_t         how,
                        void *** /*3 stars, wah!!*/ parray); /* <quote>you dont't mess around
                                                                       do you you ?</quote> */

/** avltree_printfun_t
 * this function must return the number of chars printed (let be it small for better view) */
typedef int         (*avltree_printfun_t)(
                        FILE *                      out,
                        const avltree_node_t *      node);

/** avltree_print()
 * complexity: O(n) */
void                avltree_print(
                        avltree_t *                 tree,
                        avltree_printfun_t          print,
                        FILE * out);

/** default avltree_node_printer */
int                 avltree_print_node_default(
                        FILE *                      out,
                        const avltree_node_t *      node);


/*****************************
 * AVL NODE TESTS OPERATIONS *
 * ***************************/

#if !defined(VLIB_AVLTREE_NODE_TESTS)
# define VLIB_AVLTREE_NODE_TESTS 0
#endif
#if VLIB_AVLTREE_NODE_TESTS
typedef struct {
    size_t  node_size;
    size_t  left_offset;
    size_t  right_offset;
    size_t  data_offset;
    size_t  balance_offset;
    char    optimize_bits;
    char    posix_memalign_fallback;
} avltree_node_info_t;
/** get infos about avltree_node_t: size, offsets, ... */
void                avltree_node_infos(avltree_node_info_t * infos);
/** get left child pointer of an avltree_node_t
 * !! This function is provided for the tests but SHOULD NOT be used otherwise */
avltree_node_t **   avltree_node_left_ptr(avltree_node_t * node);
/** get right child pointer of an avltree_node_t
 * !! This function is provided for the tests but SHOULD NOT be used otherwise */
avltree_node_t **   avltree_node_right_ptr(avltree_node_t * node);
/** set a node pointer (avltree_node_t **)
 * !! This function is provided for the tests but SHOULD NOT be used otherwise */
void                avltree_node_set(avltree_node_t ** nodeptr, avltree_node_t * new);
/** get a node value from its pointer (avltree_node_t **)
 * !! This function is provided for the tests but SHOULD NOT be used otherwise */
avltree_node_t *    avltree_node_get(avltree_node_t ** nodeptr);
#endif // ! #if VLIB_AVLTREE_NODE_TESTS

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

