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
    char                new_balance;    /* OUT : new balance of inserted node */
    avltree_node_t *    prev_child;     /* internal: keep previous child when retracing parents */
} avltree_visit_insert_t;

/*****************************************************************************/
static avltree_node_t * avltree_rotate_left(avltree_t * tree, avltree_node_t * node);
static avltree_node_t * avltree_rotate_right(avltree_t * tree, avltree_node_t * node);
static int              avltree_visit_rebalance(
                            avltree_t *                 tree,
                            avltree_node_t *            node,
                            avltree_visit_context_t *   context,
                            void *                      user_data);
static int              avltree_visit_insert(
                            avltree_t *                 tree,
                            avltree_node_t *            node,
                            avltree_visit_context_t *   context,
                            void *                      user_data);
static int              avltree_visit_free(
                            avltree_t *                 tree,
                            avltree_node_t *            node,
                            avltree_visit_context_t *   context,
                            void *                      user_data);

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

/*****************************************************************************/
avltree_node_t *    avltree_insert(
                        avltree_t *                 tree,
                        void *                      data) {
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
    insert_data.newdata = data;
    if (avltree_visit(tree, avltree_visit_insert, &insert_data, AVH_PREFIX | AVH_SUFFIX)
            == AVS_FINISHED) {
        return insert_data.newnode;
    }
    return NULL;
}

/*****************************************************************************/
void                avltree_free(
                        avltree_t *                 tree) {
    if (tree == NULL) {
        return ;
    }
    avltree_visit(tree, avltree_visit_free, NULL, AVH_PREFIX);
    if (tree->stack != NULL && (tree->flags & AFL_SHARED_STACK) != 0) {
        rbuf_free(tree->stack);
    }
    free(tree);
}

/*****************************************************************************/
void *              avltree_find(
                        avltree_t *                 tree,
                        const void *                data) {
    //TODO
    return NULL;
}

/*****************************************************************************/
int                 avltree_visit(
                        avltree_t *                 tree,
                        avltree_visitfun_t          visit,
                        void *                      user_data,
                        avltree_visit_how_t         how) {
    rbuf_t *                stack       = NULL;
    int                     ret         = AVS_FINISHED;
    avltree_visit_context_t context;

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
        context.level = 0;
        context.index = 0;
        context.stack = stack;
        context.how = (how & AVH_BREADTH) != 0 ? (how & ~AVH_RIGHT) : AVH_PREFIX;
    }

    while (rbuf_size(stack) != 0) {
        avltree_node_t *      node = (avltree_node_t *) ((how & AVH_BREADTH) != 0 ?
                                                            rbuf_dequeue(stack) :
                                                            rbuf_pop(stack));
        avltree_node_t *      right = node->right;
        avltree_node_t *      left = node->left;

        /* visit the current node */
        if ((how & ~AVH_RIGHT & context.how) == context.how) {
            ret = visit(tree, node, &context, user_data);
        }

        /* stop on error or when visit goal is accomplished */
        if (ret == AVS_ERROR || ret == AVS_FINISHED) {
            break ;
        }

        switch (context.how) {
            case AVH_PREFIX:
            case AVH_BREADTH:
                /* push left/right child if required */
                if (((how & (AVH_BREADTH | AVH_RIGHT)) == (AVH_BREADTH | AVH_RIGHT))
                ||  ((how & (AVH_RIGHT | AVH_BREADTH)) == 0)) {
                    if (right != NULL && (ret == AVS_GO_RIGHT || ret == AVS_CONTINUE)) {
                        rbuf_push(stack, right);
                    }
                    if (left != NULL && (ret == AVS_GO_LEFT || ret == AVS_CONTINUE)) {
                        rbuf_push(stack, left);
                    }
                } else {
                    if (left != NULL && (ret == AVS_GO_LEFT || ret == AVS_CONTINUE)) {
                        rbuf_push(stack, left);
                    }
                    if (right != NULL && (ret == AVS_GO_RIGHT || ret == AVS_CONTINUE)) {
                        rbuf_push(stack, right);
                    }
                }
                break ;
            case AVH_INFIX:
                break ;
            case AVH_SUFFIX:
                break ;
            default:
                LOG_ERROR(g_vlib_log, "avltree_visit: bad state %d", context.how);
                ret = AVS_ERROR;
                rbuf_reset(stack);
                break ;
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
                        avltree_t *                 tree,
                        const void *                data) {
    if (tree == NULL) {
        return NULL;
    }
    //TODO
    return NULL;
}

/*****************************************************************************/
static int          avltree_visit_rebalance(
                        avltree_t *                 tree,
                        avltree_node_t *            node,
                        avltree_visit_context_t *   context,
                        void *                      user_data) {

    avltree_visit_insert_t *    idata       = (avltree_visit_insert_t *) user_data;
    avltree_node_t *            parent      = rbuf_top(context->stack);
    avltree_node_t **           pparent;
    int                         balance     = node->left == idata->prev_child ? -1 : +1;
    (void) context;

    pparent = parent ? (parent->left == node ? &parent->left : &parent->right) : &tree->root;

    LOG_DEBUG(g_vlib_log, "INSERT %ld parent %ld obal:%d nbal:%d ppar=%ld",
              (long)idata->newnode->data, (long) node->data, node->balance,
              node->balance + balance, (long)(*pparent)->data);

    node->balance += balance;
#if 1
    if (node->balance < -1) {
        /* right rotate */
        if (node->left->balance > 0) {
            //double rotate
            node->left = avltree_rotate_left(tree, node->left);
            LOG_DEBUG(g_vlib_log, "left right rotation");
        } else {
            LOG_DEBUG(g_vlib_log, "right rotation");
        }
        *pparent = avltree_rotate_right(tree, node);
        return AVS_FINISHED;
    } else if (node->balance > 1) {
        // left rotate
        if (node->right->balance < 0) {
            //double rotate
            node->right = avltree_rotate_right(tree, node->right);
            LOG_DEBUG(g_vlib_log, "right left rotation");
        } else {
            LOG_DEBUG(g_vlib_log, "left rotation");
        }
        *pparent = avltree_rotate_left(tree, node);
        return AVS_FINISHED;
    }
#endif
    idata->prev_child = node;

    return AVS_CONTINUE;
}

/*****************************************************************************/
static int          avltree_visit_insert(
                        avltree_t *                 tree,
                        avltree_node_t *            node,
                        avltree_visit_context_t *   context,
                        void *                      user_data) {
    avltree_visit_insert_t *    idata = (avltree_visit_insert_t *) user_data;
    avltree_node_t *            new;
    avltree_node_t **           parent;

    if (context->how == AVH_INFIX) {
        return AVS_NEXTVISIT;
    } else if (context->how == AVH_SUFFIX) {
        return avltree_visit_rebalance(tree, node, context, user_data);
    } else if (context->how != AVH_PREFIX) {
        return AVS_ERROR;
    }

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
    idata->prev_child = new;

    LOG_DEBUG(g_vlib_log, "inserted node 0x%lx data %ld left:0x%lx right:0x%lx "
                          "node:0x%lx node->left:0x%lx node->right:0x%lx\n parent 0x%lx",
              (unsigned long)new, (long)new->data, (unsigned long)new->left,
              (unsigned long)new->right, (unsigned long)node,
              (unsigned long)node->left, (unsigned long)node->right,
              (unsigned long)*parent);

    if (idata->new_balance == 0) {
        return AVS_FINISHED;
    }
    /* stop prefix visit and switch to suffix visit */
    return AVS_NEXTVISIT;
}

/*****************************************************************************/
static int          avltree_visit_free(
                        avltree_t *                 tree,
                        avltree_node_t *            node,
                        avltree_visit_context_t *   context,
                        void *                      user_data) {
    (void) context;
    (void) user_data;

    avltree_node_free(tree, node);
    return AVS_CONTINUE;
}

/*****************************************************************************/
/** rotate_left()
 * the right child of node is right-heavy (balance = 2).
 * @param tree the tree
 * @param node root of subtree to be rotated
 */
static avltree_node_t * avltree_rotate_left(avltree_t * tree, avltree_node_t * node) {
    (void) tree;
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
    (void) tree;
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

