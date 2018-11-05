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
#include "vlib/rbuf.h"

#include "vlib_private.h"

/*****************************************************************************/
#define AVLTREE_STACK_SZ    32

typedef struct {
    void *              newdata;        /* IN  : value of new node */
    avltree_node_t *    newnode;        /* OUT : pointer to new created node */
    rbuf_t *            stack;          /* FIXME not really needed */
    char                new_balance;    /* OUT : new balance of inserted node */
} avltree_visit_insert_t;

/*****************************************************************************/
static avltree_node_t * avltree_rotate_left(avltree_t * tree, avltree_node_t * node);
static avltree_node_t * avltree_rotate_right(avltree_t * tree, avltree_node_t * node);

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
static int          avltree_visit_insert(
                        avltree_t *             tree,
                        avltree_node_t *        node,
                        avltree_visit_data_t *  vdata) {
    avltree_visit_insert_t *    idata = vdata ? (avltree_visit_insert_t *) vdata->user_data : NULL;
    avltree_node_t *            new;
    avltree_node_t **           parent;
    //char                        new_balance = 0;

    LOG_DEBUG(g_vlib_log, "visiting node 0x%lx data %ld left:0x%lx right:0x%lx "
                          "left_data:0x%ld right_data:0x%ld\n",
              (unsigned long)node, (long) node->data,
              (unsigned long)node->left, (unsigned long)node->right,
              node->left?(long)node->left->data:-1, node->right?(long)node->right->data:-1);

    if (tree->cmp(idata->newdata, node->data) <= 0) {
        /* go left */
        if (node->left != NULL) {
            /* continue with left node */
            return AVS_GO_LEFT;
        }
        /* left is null, we insert on node->left */
        parent = &(node->left);
        if (node->right == NULL) {
            //new_balance = -1;
            idata->new_balance = -1;
        }
    } else {
        /* go right */
        if (node->right != NULL) {
            /* continue with right node */
            return AVS_GO_RIGHT;
        }
        /* right is null, we insert on node->right */
        parent = &(node->right);
        if (node->left == NULL) {
            //new_balance = 1;
            idata->new_balance = +1;
        }
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

    if (idata->new_balance == 0) {
        return AVS_FINISHED;
    }

    fprintf(stderr, "parent_balance: %d stacksz: %lu\n", node->balance, rbuf_size(idata->stack));
    avltree_node_t * chld = new;
    for (ssize_t i = rbuf_size(idata->stack) - 1; i >= 0; i--) {
        node = (avltree_node_t*) rbuf_get(idata->stack, i);
        avltree_node_t * par = i > 0 ? rbuf_get(idata->stack, i-1) : NULL;
        avltree_node_t ** ppar = par ? par->left == node ? &par->left : &par->right : &tree->root;
        int bal = node->left == chld ? -1 : +1;

        fprintf(stderr, "INSERT %ld parent %ld obal:%d nbal:%d ppar=%ld\n",
                (long)new->data, (long) node->data, node->balance, node->balance + bal,
                (long)(*ppar)->data);
        node->balance += bal;
        chld = node;
    }

    LOG_DEBUG(g_vlib_log, "inserted node 0x%lx data %ld left:0x%lx right:0x%lx "
                          "node:0x%lx node->left:0x%lx node->right:0x%lx\n parent 0x%lx",
              (unsigned long)new, (long)new->data, (unsigned long)new->left,
              (unsigned long)new->right, (unsigned long)node,
              (unsigned long)node->left, (unsigned long)node->right,
              (unsigned long)*parent);

    return AVS_FINISHED;
}

/*****************************************************************************/
static int          avltree_visit_free(
                        avltree_t *             tree,
                        avltree_node_t *        node,
                        avltree_visit_data_t *  vdata) {
    (void) vdata;
    avltree_node_free(tree, node);
    return AVS_CONT;
}

/*****************************************************************************/
avltree_t *         avltree_create(
                        avltree_flags_t         flags,
                        avltree_cmpfun_t        cmpfun,
                        avltree_freefun_t       freefun) {
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
        tree->stack = rbuf_create(AVLTREE_STACK_SZ, RBF_DEFAULT);
        if (tree->stack == NULL) {
            free(tree);
            return NULL;
        }
    } else {
        tree->stack = NULL;
    }
    tree->root = NULL;
    tree->flags = flags;
    tree->cmp = cmpfun;
    tree->free = freefun;
    return tree;
}

/*****************************************************************************/
avltree_node_t *    avltree_node_create(
                        avltree_t *             tree,
                        void *                  data,
                        avltree_node_t *        left,
                        avltree_node_t *        right) {
    avltree_node_t * new = avltree_node_alloc(tree);

    if (new == NULL)
        return NULL;
    new->data = data;
    new->right = right;
    new->left = left;
    new->balance = 0;
    return new;
}

/*****************************************************************************/
avltree_node_t *    avltree_insert(
                        avltree_t *             tree,
                        void *                  data) {
    avltree_visit_insert_t    insert_data;

    if (tree == NULL) {
        return NULL;
    }
    if (tree->root == NULL) {
        /* particular case when root is NULL : allocate it here without visiting */
        if ((tree->root = avltree_node_create(tree, data, NULL, NULL)) == NULL) {
            return NULL;
        }
        LOG_DEBUG(g_vlib_log, "created root 0x%lx data 0x%ld left:0x%lx right:0x%lx\n",
                  (unsigned long)tree->root, (long)tree->root->data,
                  (unsigned long)tree->root->left, (unsigned long)tree->root->right);
        return tree->root;
    }
    insert_data.stack = rbuf_create(AVLTREE_STACK_SZ, RBF_DEFAULT);
    insert_data.newdata = data;
    if (avltree_visit(tree, avltree_visit_insert, &insert_data, AVH_PREFIX_L) == AVS_FINISHED) {
        rbuf_free(insert_data.stack);
        return insert_data.newnode;
    }
    rbuf_free(insert_data.stack);
    return NULL;
}

/*****************************************************************************/
void                avltree_free(
                        avltree_t *             tree) {
    if (tree == NULL) {
        return ;
    }
    avltree_visit(tree, avltree_visit_free, NULL, AVH_DEFAULT);
    free(tree);
}

/*****************************************************************************/
void *              avltree_find(
                        avltree_t *             tree,
                        const void *            data) {
    //TODO
    return NULL;
}

/*****************************************************************************/
int                 avltree_visit(
                        avltree_t *             tree,
                        avltree_visitfun_t      visit,
                        void *                  user_data,
                        avltree_visit_how_t     how) {
    rbuf_t *                stack       = NULL;
    int                     ret         = AVS_FINISHED;
    avltree_visit_data_t    visit_data;

    if (tree == NULL || visit == NULL) {
        return AVS_ERROR;
    }
    if ((tree->flags & AFL_SHARED_STACK) != 0) {
        stack = tree->stack;
    } else {
        stack = rbuf_create(AVLTREE_STACK_SZ, RBF_DEFAULT);
    }
    if (tree->root != NULL) {
        rbuf_push(stack, tree->root);
        visit_data.level = 0;
        visit_data.index = 0;
        visit_data.stack = stack;
        visit_data.user_data = user_data;
    }
    while (rbuf_size(stack) != 0) {
        avltree_node_t *      node = (avltree_node_t *) (how == AVH_LARG_L || how == AVH_LARG_R ?
                                                            rbuf_dequeue(stack) :
                                                            rbuf_pop(stack));
        avltree_node_t *      right = node->right;
        avltree_node_t *      left = node->left;

        /* visit the current node */
        ret = visit(tree, node, &visit_data); // FIXME: must take care of how

        /* stop on error or when visit goal is accomplished */
        if (ret == AVS_ERROR || ret == AVS_FINISHED) {
            break ;
        }

        /* push left/right child if required */
        if (how == AVH_INFIX_L || how == AVH_PREFIX_L || how == AVH_SUFFIX_L || how == AVH_LARG_R) {
            if (right != NULL && (ret == AVS_GO_RIGHT || ret == AVS_CONT)) {
                rbuf_push(stack, right);
            }
            if (left != NULL && (ret == AVS_GO_LEFT || ret == AVS_CONT)) {
                rbuf_push(stack, left);
            }
        } else {
            if (left != NULL && (ret == AVS_GO_LEFT || ret == AVS_CONT)) {
                rbuf_push(stack, left);
            }
            if (right != NULL && (ret == AVS_GO_RIGHT || ret == AVS_CONT)) {
                rbuf_push(stack, right);
            }
        }
    }
    if ((tree->flags & AFL_SHARED_STACK) != 0) {
        rbuf_reset(stack);
    } else {
        rbuf_free(stack);
    }
    return ret != AVS_ERROR ? AVS_FINISHED : AVS_ERROR;
}

/*****************************************************************************/
void *              avltree_remove(
                        avltree_t *             tree,
                        const void *            data) {
    //TODO
    return NULL;
}

/*****************************************************************************/
/** rotate_left()
 * the right child of node is right-heavy (balance = 2).
 * @param tree the tree
 * @param node root of subtree to be rotated
 */
static avltree_node_t * avltree_rotate_left(avltree_t * tree, avltree_node_t * node) {
    avltree_node_t * rchild = node->right;
    node->right = rchild->left;
    rchild->left = node;

    if (rchild->balance == 0) {
        node->balance = 1;
        rchild->balance = -1;
    } else {
        node->balance = 0;
        rchild->balance = 0;
    }
    return rchild;
}

static avltree_node_t * avltree_rotate_right(avltree_t * tree, avltree_node_t * node) {
    avltree_node_t * lchild = node->left;
    node->left = lchild->right;
    lchild->right = node;

    if (lchild->balance == 0) {
        node->balance = -1;
        lchild->balance = +1;
    } else {
        node->balance = 0;
        lchild->balance = 0;
    }
    return lchild;
}


