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
#include "vlib/log.h"
#include "vlib/slist.h"

#include "vlib_private.h"

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

    LOG_DEBUG(g_vlib_log, "visiting node 0x%lx data %ld left:0x%lx right:0x%lx "
                          "left_data:0x%ld right_data:0x%ld\n",
              (unsigned long)node, (long) node->data,
              (unsigned long)node->left, (unsigned long)node->right,
              node->left?(long)node->left->data:-1, node->right?(long)node->right->data:-1);

    if (tree->cmp(insert_data->data, node->data) <= 0) {
        /* go left */
        if (node->left != NULL && tree->cmp(insert_data->data, node->left->data) <= 0) { // FIXME
            /* need to visit left node because its value is greater than data */
            return AVS_GO_LEFT;
        }
        /* left is null, we insert on node->left */
        parent = &(node->left);
    } else {
        /* go right */
        if (node->right != NULL && tree->cmp(insert_data->data, node->right->data) > 0) { // FIXME
            /* need to visit right node because its value is smaller than data */
            return AVS_GO_RIGHT;
        }
        /* right is null, we insert on node->right */
        parent = &(node->right);
    }
    new = avltree_node_alloc(tree);
    if (new == NULL)
        return AVS_ERROR;
    if (parent == &node->right) {   //FIXME
        new->left = (*parent);
        new->right = NULL;
    } else {                        //FIXME
        new->right = (*parent);
        new->left = NULL;
    }
    new->data = insert_data->data;
    (*parent) = new;
    insert_data->node = new;

    LOG_DEBUG(g_vlib_log, "inserted node 0x%lx data %ld left:0x%lx right:0x%lx "
                          "node:0x%lx node->left:0x%lx node->right:0x%lx\n parent 0x%lx",
              (unsigned long)new, (long)new->data, (unsigned long)new->left,
              (unsigned long)new->right, (unsigned long)node,
              (unsigned long)node->left, (unsigned long)node->right,
              (unsigned long)*parent);

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
    if (tree->root == NULL) {
        /* particular case when root is NULL : allocate it here without visiting */
        tree->root = avltree_node_alloc(tree);
        tree->root->left = NULL;
        tree->root->right = NULL;
        tree->root->data = data;
        LOG_DEBUG(g_vlib_log, "created root 0x%lx data 0x%ld left:0x%lx right:0x%lx\n",
                  (unsigned long)tree->root, (long)tree->root->data,
                  (unsigned long)tree->root->left, (unsigned long)tree->root->right);
        return tree->root;
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
    if (tree->root != NULL) {
        stack = slist_prepend(stack, tree->root);
    }
    while (stack != NULL) {
        avltree_node_t *      node = (avltree_node_t *) stack->data;
        avltree_node_t *      right = node->right;
        avltree_node_t *      left = node->left;
        /* visit the current node */
        ret = visit(tree, node, visit_data); // FIXME: must take care of how
        /* pop current node from the stack */
        stack = slist_remove_ptr(stack, stack->data);
        /* stop on error or when visit goal is accomplished */
        if (ret == AVS_ERROR || ret == AVS_FINISHED) {
            break ;
        }
        /* push node->left if required */
        if (left != NULL && (ret == AVS_GO_LEFT || ret == AVS_CONT)) {
            stack = slist_prepend(stack, left);
        }
        /* push node->right if required */
        if (right != NULL && (ret == AVS_GO_RIGHT || ret == AVS_CONT)) {
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


