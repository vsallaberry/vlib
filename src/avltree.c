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
    insert_data.newnode = NULL;
    if (avltree_visit(tree, avltree_visit_insert, &insert_data, AVH_PREFIX | AVH_SUFFIX)
            == AVS_FINISHED) {
        /*rbuf_free(insert_data.stack);
        / * insertion done, update balance and rebalance if needed * /
        fprintf(stderr, "update_balance for %ld\n", (long) data);
        if (0 && insert_data.new_balance != 0
        &&  avltree_visit(tree, avltree_visit_update_balance, &insert_data, AVH_PREFIX_L)
                != AVS_FINISHED) {
            LOG_ERROR(g_vlib_log, "error while updating balance after %ld insertion",
                      (long) data);
        }*/
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
    if (avltree_visit(tree, avltree_visit_free, NULL, AVH_SUFFIX) != AVS_FINISHED) {
        LOG_WARN(g_vlib_log, "warning avltree_visit_free() error");
    }
    if (tree->stack != NULL && (tree->flags & AFL_SHARED_STACK) != 0) {
        rbuf_free(tree->stack);
    }
    free(tree);
}

/*****************************************************************************/
void *              avltree_find(
                        avltree_t *                 tree,
                        const void *                data) {
    avltree_node_t *    node;
    int                 cmp;

    if (tree == NULL) {
        return NULL;
    }
    node = tree->root;
    while (node) {
        cmp = tree->cmp(data, node->data);
        if (cmp == 0) {
            return node->data;
        } else if (cmp < 0) {
            node = node->left;
        } else {
            node = node->right;
        }
    }
    //TODO: handle nodes with same value;
    return NULL;
}

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
        if (ret == AVS_GO_LEFT) {
           ret = AVS_GO_RIGHT;
        } else if (ret == AVS_GO_RIGHT) {
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
int                 avltree_visit(
                        avltree_t *                 tree,
                        avltree_visitfun_t          visit,
                        void *                      user_data,
                        avltree_visit_how_t         how) {
    rbuf_t *                stack       = NULL;
    int                     ret         = AVS_CONTINUE;
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
        //TODO: loop (stack mode) can be optimized by not pushing the next node
        //      to be visited, just do node = node->left/right
        //TODO: PREFIX mode can be optimized (same as BREADTH) if it is the only required in how.
        //TODO: AVS_NEXTVISIT and AVS_SKIP might not work correctly

        avltree_node_t *      right = node->right;
        avltree_node_t *      left = node->left;

        LOG_DEBUG(g_vlib_log, "altree_visit(): node:%ld(%ld,%ld) how:%d ctx:%d "
                              "do_visit:%d last_ret:%d",
                (long) node->data,
                node->left?(long)node->left->data:-1,
                node->right?(long)node->right->data:-1,
                how, context.how, (how &~AVH_RIGHT & context.how), ret);

        /* visit the current node if required, and update ret only if visitor is called */
        if ((how & ~AVH_RIGHT & context.how) == context.how) {
            context.how |= (how & AVH_RIGHT); // TODO find cleaner solution
            ret = visit(tree, node, &context, user_data);
            context.how &= ~AVH_RIGHT; // TODO find cleaner solution
            /* stop on error or when visit goal is accomplished */
            if (ret == AVS_ERROR || ret == AVS_FINISHED) {
                break ;
            }
            /* we remove the current visit type from set */
            if (ret == AVS_NEXTVISIT) {
                how &= ~context.how;
            }
        }

        /* prepare next visit */
        switch (context.how) {
            case AVH_BREADTH:
                /* push left/right child if required */
                if ((node = avltree_visit_get_child(AGC_SECOND, ret, how, left, right)) != NULL) {
                    rbuf_push(stack, node);
                }
                if ((node = avltree_visit_get_child(AGC_FIRST, ret, how, left, right)) != NULL) {
                    rbuf_push(stack, node);
                }
                break ;
            case AVH_PREFIX:
                /* push current node, so that child can return to its parent */
                rbuf_push(stack, node);
                /* push first child if required (taking care of AVH_RIGHT modifier) */
                if (ret != AVS_NEXTVISIT
                &&  (node = avltree_visit_get_child(AGC_FIRST, ret, how, left, right)) != NULL) {
                    /* visit first child in prefix mode */
                    rbuf_push(stack, node);
                } else {
                    /* stay on node for infix visit */
                    context.how = AVH_INFIX;
                }
                break ;
            case AVH_INFIX:
                /* push current node, so that child can return to its parent */
                rbuf_push(stack, node);
                /* push left/right child if required (taking care of AVH_RIGHT modifier) */
                if (ret != AVS_NEXTVISIT
                &&  (node = avltree_visit_get_child(AGC_SECOND, ret, how, left, right)) != NULL) {
                    /* go to right, for prefix visit */
                    rbuf_push(stack, node);
                    context.how = AVH_PREFIX;
                } else {
                    /* stays on node, for SUFFIX visit */
                    context.how = AVH_SUFFIX;
                }
                break ;
            case AVH_SUFFIX:
                if (ret == AVS_NEXTVISIT) {
                    //break ;
                }
                if (rbuf_size(stack)) {
                    avltree_node_t * parent = rbuf_top(stack);
                    if (node == avltree_visit_get_child(AGC_FIRST, AVS_CONTINUE, how,
                                                        parent->left, parent->right)) {
                        context.how = AVH_INFIX;
                    } else {
                        context.how = AVH_SUFFIX;
                    }
                    /* next visit is for parent */
                }
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

    LOG_DEBUG(g_vlib_log, "INSERT %ld cur %ld(%ld,%ld) obal:%d nbal:%d ppar=%ld",
                (long)idata->newnode->data, (long) node->data,
                node->left?(long)node->left->data : -1,
                node->right?(long)node->right->data : -1,
                node->balance,
                node->balance + balance, parent ? (long)(*pparent)->data : -1);

    node->balance += balance;
#if 0
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

    if ((context->how & ~AVH_RIGHT) == AVH_SUFFIX) {
        return avltree_visit_rebalance(tree, node, context, user_data);
    } else if ((context->how & ~AVH_RIGHT) != AVH_PREFIX) {
        return AVS_ERROR;
    }

    LOG_DEBUG(g_vlib_log, "VISITING for Insert, Node %ld(%ld,%ld) How %d Ptr %lx(%lx,%lx)",
              (long)node->data,
              node->left?(long)node->left->data:-1, node->right?(long)node->right->data:-1,
              context->how,
              (unsigned long) node, (unsigned long)node->left, (unsigned long)node->right);

    if (tree->cmp(idata->newdata, node->data) <= 0) {
        /* go left */
        if (node->left != NULL) {
            /* continue with left node */
            return AVS_GO_LEFT;
        }
        /* left is null, we insert on node->left */
        parent = &(node->left);
        if (node->right == NULL) {
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

    LOG_DEBUG(g_vlib_log, "INSERTED node %ld(%lx,%lx) ptr 0x%lx on %s of %ld ptr 0x%lx",
              (long)new->data, (unsigned long)new->left, (unsigned long)new->right,
              (unsigned long)new,
              *parent == node->left ? "LEFT" : "RIGHT",
              (long)node->data, (unsigned long) node);

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

