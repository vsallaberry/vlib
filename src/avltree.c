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
#include <stdlib.h>

#include "vlib/btree.h"
#include "vlib/slist.h"

typedef struct {
    btree_node_t *  node;
    void *          data;
} btree_visit_insert_t;

static void btree_avl_rotate(btree_t * tree);

static btree_node_t * btree_node_alloc(btree_t * tree) {
    (void)tree;
    return malloc(sizeof(btree_node_t));
}

static void btree_node_free(btree_t * tree, btree_node_t * node) {
    if (tree && node) {
        if (tree->free) {
            tree->free(node->data);
        }
        free(node);
    }
}

static int btree_visit_insert(btree_t * tree, btree_node_t * node, void * data) {
    btree_visit_insert_t *      insert_data = (btree_visit_insert_t *) data;
    btree_node_t *              new;
    btree_node_t **             parent;

    if (tree->cmp(data, node->data) <= 0) {
        /* go left */
        if (node->left != NULL && tree->cmp(data, node->left->data) <= 0) { // FIXME
            /* need to visit left node because its value is greater than data */
            return BVS_GO_LEFT;
        }
        /* left is null, we insert on node->left */
        parent = &node->left;
    } else {
        /* go right */
        if (node->right != NULL && tree->cmp(data, node->right->data) > 0) { // FIXME
            /* need to visit right node because its value is smaller than data */
            return BVS_GO_RIGHT;
        }
        /* right is null, we insert on node->right */
        parent = &node->right;
    }
    new = btree_node_alloc(tree);
    if (node == NULL)
        return BVS_ERROR;
    new->left = NULL; //FIXME
    new->right = NULL; //FIXME
    new->data = insert_data->data;
    (*parent) = new;
    insert_data->node = new;
    return BVS_FINISHED;
}

static int btree_visit_free(btree_t * tree, btree_node_t * node, void * data) {
    (void) data;
    btree_node_free(tree, node);
    return BVS_CONT;
}

btree_t *       btree_create(
                    btree_flags_t       flags,
                    btree_cmpfun_t      cmpfun,
                    btree_freefun_t     freefun) {
    btree_t * tree;

    if (cmpfun == NULL) {
        return NULL; // bin tree is based on node comparisons.
    }
    tree = malloc(sizeof(btree_t));
    if (tree == NULL) {
        return NULL;
    }
    tree->root = NULL;
    tree->flags = flags;
    tree->cmp = cmpfun;
    tree->free = freefun;
    return tree;
}

btree_node_t *  btree_insert(
                    btree_t *           tree,
                    void *              data) {
    btree_visit_insert_t    insert_data;

    if (tree == NULL) {
        return NULL;
    }
    insert_data.data = data;
    if (btree_visit(tree, btree_visit_insert, &insert_data, BVH_DEFAULT) == BVS_FINISHED) {
        return insert_data.node;
    }
    return NULL;
}

void            btree_free(
                    btree_t *           tree) {
    if (tree == NULL) {
        return ;
    }
    btree_visit(tree, btree_visit_free, NULL, BVH_DEFAULT);
}

void *          btree_find(
                    btree_t *           tree,
                    const void *        data) {
    return NULL;
}

int             btree_visit(
                    btree_t *           tree,
                    btree_visitfun_t    visit,
                    void *              visit_data,
                    btree_visit_how_t   how) {
    slist_t *   stack = NULL;
    int         ret = BVS_FINISHED;

    if (tree == NULL || visit == NULL) {
        return BVS_ERROR;
    }
    stack = slist_prepend(stack, tree->root);
    while (stack != NULL) {
        btree_node_t *      node = (btree_node_t *) stack->data;
        btree_node_t *      right = node->right;
        btree_node_t *      left = node->left;
        /* visit the current node */
        ret = visit(tree, node, visit_data);
        /* pop current node from the stack */
        stack = slist_remove_ptr(stack->data, NULL);
        /* stop on error or when visit goal is accomplished */
        if (ret == BVS_ERROR || ret == BVS_FINISHED) {
            break ;
        }
        /* push node->left if required */
        if (ret == BVS_GO_LEFT || ret == BVS_CONT) {
            stack = slist_prepend(stack, left);
        }
        /* push node->right if required */
        if (ret == BVS_GO_RIGHT || ret == BVS_CONT) {
            stack = slist_prepend(stack, right);
        }
    }
    slist_free(stack, NULL);
    return ret != BVS_ERROR ? BVS_FINISHED : BVS_ERROR;
}

static void btree_avl_rotate(btree_t * tree) {

}

