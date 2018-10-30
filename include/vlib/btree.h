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
 * Simple bintree utilities.
 */
#ifndef VLIB_BTREE_H
#define VLIB_BTREE_H

#ifdef __cplusplus
extern "C" {
#endif

/** btree_node_t data compare function, strcmp-like */
typedef int     (*btree_cmpfun_t)   (const void *, const void *);
/** btree_node_t data free function, free-like */
typedef void    (*btree_freefun_t)  (void *);

/** btree_node_t */
typedef struct btree_node_s {
    void *                      data;
    struct btree_node_s *       left;
    struct btree_node_s *       right;
} btree_node_t;

/** how to visit the tree (direction) */
typedef enum {
    BF_DEFAULT,
} btree_flags_t;


/** btree_t */
typedef struct {
    struct btree_node_t *       root;
    btree_flags_t               flags;
    btree_cmpfun_t              cmp;
    btree_freefun_t             free;
} btree_t;

/** btree_visitfun_t return value */
enum {
    BVS_GO_LEFT     = -24, /* only go to left node, but continue visit */
    BVS_GO_RIGHT    = -23, /* only go to right node, but continue visit */
    BVS_SKIP        = -22, /* skip this node, but continue visit */
    BVS_CONT        = -21, /* continue visit */
    BVS_ERROR       = -1,  /* stop visit, report error */
    BVS_FINISHED    = 0,   /* stop visit, report success */
} btree_visit_status_t;

/** btree_node_t visit function called on each node by btree_visit()
 * @param tree the tree beiing visited, cannot be NULL (ensured by btree_visit())
 * @param node the node beiing visited, cannot be NULL (ensured by btree_visit())
 * @param data specific data for visit function
 * @return btree_visit_status_t
 */
typedef int     (*btree_visitfun_t) (btree_t * tree, btree_node_t * node, void * data);

/** how to visit the tree (direction) */
typedef enum {
    BVH_LEFT_LEAF   = 0,
    BVH_RIGHT_LEAF,
    BVH_LEFT_RIGHT,
    BVH_RIGHT_LEFT,
    BVH_DEFAULT = BVH_LEFT_RIGHT,
} btree_visit_how_t;

/** btree_create() */
btree_t *       btree_create(
                    btree_flags_t       flags,
                    btree_cmpfun_t      cmpfun,
                    btree_freefun_t     freefun);

/** btree_insert() */
btree_node_t *  btree_insert(
                    btree_t *           tree,
                    void *              data);

/** btree_free() */
void            btree_free(
                    btree_t *           tree);

/** btree_find() */
void *          btree_find(
                    btree_t *           tree,
                    const void *        data);

/** btree_visit() */
int             btree_visit(
                    btree_t *           tree,
                    btree_visitfun_t    visit,
                    void *              visit_data,
                    btree_visit_how_t   how);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

