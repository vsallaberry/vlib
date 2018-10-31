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
 * Simple avl tree utilities.
 * AVL: 1962, Georgii Adelson-Velsky & Evguenii Landis.
 */
#include <stdlib.h>

#include "vlib/avltree.h"
#include "vlib/slist.h"

/*****************************************************************************/
typedef struct {
    avltree_node_t *    node;
    void *              data;
} avltree_visit_insert_t;

/*****************************************************************************/
static void avltree_avl_rotate(avltree_t * tree);

/*****************************************************************************/
static avltree_node_t * avltree_node_alloc(avltree_t * tree) {
    (void)tree;
    return malloc(sizeof(avltree_node_t));
}

/*****************************************************************************/
static void avltree_node_free(avltree_t * tree, avltree_node_t * node) {
    if (tree && node) {
        if (tree->free) {
            tree->free(node->data);
        }
        free(node);
    }
}

/*****************************************************************************/
static int avltree_visit_insert(avltree_t * tree, avltree_node_t * node, void * data) {
    avltree_visit_insert_t *      insert_data = (avltree_visit_insert_t *) data;
    avltree_node_t *              new;
    avltree_node_t **             parent;

    if (tree->cmp(data, node->data) <= 0) {
        /* go left */
        if (node->left != NULL && tree->cmp(data, node->left->data) <= 0) { // FIXME
            /* need to visit left node because its value is greater than data */
            return AVS_GO_LEFT;
        }
        /* left is null, we insert on node->left */
        parent = &node->left;
    } else {
        /* go right */
        if (node->right != NULL && tree->cmp(data, node->right->data) > 0) { // FIXME
            /* need to visit right node because its value is smaller than data */
            return AVS_GO_RIGHT;
        }
        /* right is null, we insert on node->right */
        parent = &node->right;
    }
    new = avltree_node_alloc(tree);
    if (node == NULL)
        return AVS_ERROR;
    new->left = NULL; //FIXME
    new->right = NULL; //FIXME
    new->data = insert_data->data;
    (*parent) = new;
    insert_data->node = new;
    return AVS_FINISHED;
}

/*****************************************************************************/
static int avltree_visit_free(avltree_t * tree, avltree_node_t * node, void * data) {
    (void) data;
    avltree_node_free(tree, node);
    return AVS_CONT;
}

/*****************************************************************************/
avltree_t *       avltree_create(
                    avltree_flags_t     flags,
                    avltree_cmpfun_t    cmpfun,
                    avltree_freefun_t   freefun) {
    avltree_t * tree;

    if (cmpfun == NULL) {
        return NULL; // bin tree is based on node comparisons.
    }
    tree = malloc(sizeof(avltree_t));
    if (tree == NULL) {
        return NULL;
    }
    tree->root = NULL;
    tree->flags = flags;
    tree->cmp = cmpfun;
    tree->free = freefun;
    return tree;
}

/*****************************************************************************/
avltree_node_t *  avltree_insert(
                    avltree_t *         tree,
                    void *              data) {
    avltree_visit_insert_t    insert_data;

    if (tree == NULL) {
        return NULL;
    }
    insert_data.data = data;
    if (avltree_visit(tree, avltree_visit_insert, &insert_data, AVH_DEFAULT) == AVS_FINISHED) {
        return insert_data.node;
    }
    return NULL;
}

/*****************************************************************************/
void            avltree_free(
                    avltree_t *         tree) {
    if (tree == NULL) {
        return ;
    }
    avltree_visit(tree, avltree_visit_free, NULL, AVH_DEFAULT);
}

/*****************************************************************************/
void *          avltree_find(
                    avltree_t *         tree,
                    const void *        data) {
    //TODO
    return NULL;
}

/*****************************************************************************/
int             avltree_visit(
                    avltree_t *         tree,
                    avltree_visitfun_t  visit,
                    void *              visit_data,
                    avltree_visit_how_t how) {
    slist_t *   stack = NULL;
    int         ret = AVS_FINISHED;

    if (tree == NULL || visit == NULL) {
        return AVS_ERROR;
    }
    stack = slist_prepend(stack, tree->root);
    while (stack != NULL) {
        avltree_node_t *      node = (avltree_node_t *) stack->data;
        avltree_node_t *      right = node->right;
        avltree_node_t *      left = node->left;
        /* visit the current node */
        ret = visit(tree, node, visit_data); // FIXME: must take care of how
        /* pop current node from the stack */
        stack = slist_remove_ptr(stack->data, NULL);
        /* stop on error or when visit goal is accomplished */
        if (ret == AVS_ERROR || ret == AVS_FINISHED) {
            break ;
        }
        /* push node->left if required */
        if (ret == AVS_GO_LEFT || ret == AVS_CONT) {
            stack = slist_prepend(stack, left);
        }
        /* push node->right if required */
        if (ret == AVS_GO_RIGHT || ret == AVS_CONT) {
            stack = slist_prepend(stack, right);
        }
    }
    slist_free(stack, NULL);
    return ret != AVS_ERROR ? AVS_FINISHED : AVS_ERROR;
}

/*****************************************************************************/
void *          avltree_remove(
                    avltree_t *         tree,
                    const void *        data) {
    //TODO
    return NULL;
}

/*****************************************************************************/
static void avltree_avl_rotate(avltree_t * tree) {
    //TODO
}


