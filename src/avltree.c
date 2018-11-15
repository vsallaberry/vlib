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
#include <stdio.h>
#include <errno.h>

#include "vlib/avltree.h"
#include "vlib/log.h"
#include "vlib/slist.h"
#include "vlib/rbuf.h"

#include "vlib_private.h"

/*****************************************************************************/
#define AVLTREE_STACK_SZ    32 /* 10^5 elts:depth=20, 10^6:24, 10^7:28 */

typedef struct {
    void *              newdata;        /* IN  : value of new node */
    avltree_node_t *    newnode;        /* OUT : pointer to new created node */
    char                new_balance;    /* OUT : new balance of inserted node */
    avltree_node_t *    prev_child;     /* internal: keep previous child when retracing parents */
} avltree_visit_insert_t;

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
static int              avltree_visit_free(
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
static inline void avltree_node_free(avltree_t * tree, avltree_node_t * node) {
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
        tree->stack = rbuf_create(AVLTREE_STACK_SZ, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
        if (tree->stack == NULL) {
            free(tree);
            return NULL;
        }
    } else {
        tree->stack = NULL;
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
        ++tree->n_elements;
        LOG_DEBUG(g_vlib_log, "created root 0x%lx data 0x%ld left:0x%lx right:0x%lx\n",
                  (unsigned long)tree->root, (long)tree->root->data,
                  (unsigned long)tree->root->left, (unsigned long)tree->root->right);
        return tree->root;
    }
    insert_data.newdata = data;
    insert_data.newnode = NULL;
    insert_data.prev_child = NULL;
    insert_data.new_balance = 0;
    if (avltree_visit(tree, avltree_visit_insert, &insert_data, AVH_PREFIX | AVH_SUFFIX)
            == AVS_FINISHED) {
        ++tree->n_elements;
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
    if (avltree_visit(tree, avltree_visit_free, NULL, AVH_PREFIX) != AVS_FINISHED) {
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
        errno = EINVAL;
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
    errno = ENOENT;
    return NULL;
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
    return node->data;
}

/*****************************************************************************/
unsigned int        avltree_find_depth(
                        avltree_t *                 tree) {
    avltree_node_t *    node;
    unsigned int        depth = 0;

    if (tree == NULL) {
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
    return depth;
}
/*****************************************************************************/
size_t              avltree_count(
                        avltree_t *                 tree) {
    if (tree == NULL) {
        errno = EINVAL;
        return 0;
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
    if (tree->stack != NULL) {
       size += rbuf_memorysize(tree->stack);
    }
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
    rbuf_t *                stack           = NULL;
    avltree_visit_context_t context;
    int                     ret             = AVS_CONTINUE;
    int                     breadth_style   = 0;
    int                     push            = 1;

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
    }

    context.level = 0;
    context.index = 0;
    context.stack = stack;
    context.state = (how & AVH_BREADTH) != 0 ? (how & ~AVH_RIGHT & AVH_BREADTH) : AVH_PREFIX;
    breadth_style = (context.state == AVH_BREADTH || (how & ~AVH_RIGHT) == AVH_PREFIX);
    context.how = how;

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

        LOG_DEBUG(g_vlib_log, "altree_visit(): node:%ld(%ld,%ld) state:%d how:%d "
                              "do_visit:%d last_ret:%d how_orig:%d",
                (long) node->data,
                node->left?(long)node->left->data:-1,
                node->right?(long)node->right->data:-1,
                context.state, how, (how &~AVH_RIGHT & context.state), ret, context.how);

        /* visit the current node if required, and update ret only if visitor is called */
        if ((how & ~AVH_RIGHT & context.state) == context.state) {
            ret = visit(tree, node, &context, user_data);
            /* stop on error or when visit goal is accomplished */
            if (ret == AVS_ERROR || ret == AVS_FINISHED) {
                break ;
            }
            /* we remove the current visit type from set */
            if (ret == AVS_NEXTVISIT) {
                how &= ~context.state;
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
        switch (context.state) {
            case AVH_PREFIX:
                /* push current node, so that child can return to its parent */
                rbuf_push(stack, node);
                /* push first child if required (taking care of AVH_RIGHT modifier) */
                if (push
                &&  (node = avltree_visit_get_child(AGC_FIRST, ret, how, left, right)) != NULL) {
                    /* visit first child in prefix mode */
                    rbuf_push(stack, node);
                } else {
                    if (!push && (how & AVH_INFIX) == 0) {
                        /* stay on node for suffix visit, as neither infix nor push is required */
                        context.state = AVH_SUFFIX;
                    } else {
                        /* stay on node for infix visit */
                        context.state = AVH_INFIX;
                    }
                }
                break ;
            case AVH_INFIX:
                /* push current node, so that child can return to its parent */
                rbuf_push(stack, node);
                /* push left/right child if required (taking care of AVH_RIGHT modifier) */
                if (push
                &&  (node = avltree_visit_get_child(AGC_SECOND, ret, how, left, right)) != NULL) {
                    /* go to right, for prefix visit */
                    rbuf_push(stack, node);
                    context.state = AVH_PREFIX;
                } else {
                    /* stays on node, for SUFFIX visit */
                    context.state = AVH_SUFFIX;
                }
                break ;
            case AVH_SUFFIX:
                if (ret == AVS_NEXTVISIT) {
                    break ;
                }
                if (rbuf_size(stack)) {
                    avltree_node_t * parent = rbuf_top(stack);
                    if ((push || (how & AVH_INFIX) != 0)
                    && node == avltree_visit_get_child(AGC_FIRST, AVS_CONTINUE, how,
                                                       parent->left, parent->right)) {
                        /* prepare the parent to be visited in infix mode */
                        context.state = AVH_INFIX;
                    } else {
                        context.state = AVH_SUFFIX;
                    }
                    /* next visit is for parent */
                }
                break ;
            default:
                LOG_ERROR(g_vlib_log, "avltree_visit: bad state %d", context.state);
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
        errno = EINVAL;
        return NULL;
    }
    //TODO
    if (0) {
        --tree->n_elements;
    }
    return NULL;
}

/*****************************************************************************/
static int          avltree_visit_rebalance(
                        avltree_t *                     tree,
                        avltree_node_t *                node,
                        const avltree_visit_context_t * context,
                        void *                          user_data) {

    avltree_visit_insert_t *    idata       = (avltree_visit_insert_t *) user_data;
    avltree_node_t *            parent      = rbuf_top(context->stack);
    avltree_node_t **           pparent;
    int                         balance_dir = node->left == idata->prev_child ? -1 : +1;
    (void) context;

    pparent = parent ? (parent->left == node ? &parent->left : &parent->right) : &tree->root;

    LOG_DEBUG(g_vlib_log, "BALANCING %ld(%ld,%ld) ppar=%ld "
                          "difbal:%d dirbal:%d obal:%d nbal:%d (Inserted:%ld)",
                (long) node->data,
                node->left ? (long) node->left->data : -1,
                node->right ? (long) node->right->data : -1,
                parent ? (long)(*pparent)->data : -1,
                idata->new_balance, balance_dir,
                node->balance,
                node->balance + (balance_dir * idata->new_balance),
                (long) idata->newnode->data);

    if (idata->new_balance != 0) {
        int old_balance = node->balance;
        node->balance += (balance_dir * idata->new_balance);
        if (balance_dir == -1 && old_balance <= 0)
            idata->new_balance = 1;
        else if (balance_dir == 1 && old_balance >= 0)
            idata->new_balance = 1;
        else
            idata->new_balance = 0;
    }

    if (node->balance < -1) {
#       ifdef _DEBUG
        if (g_vlib_log && g_vlib_log->level >= LOG_LVL_DEBUG)
            avltree_print(tree, avltree_print_node_default, stderr);
#       endif
        /* right rotate */
        if (node->left->balance > 0) {
            //double rotate
            LOG_DEBUG(g_vlib_log, "left(%ld) right(%ld) rotation",
                                  (long)node->left->data, (long) node->data);
            *pparent = avltree_rotate_leftright(tree, node);
        } else {
            LOG_DEBUG(g_vlib_log, "right(%ld) rotation", (long) node->data);
            *pparent = avltree_rotate_right(tree, node, 1);
        }
        idata->prev_child = *pparent;

        return AVS_FINISHED;
    } else if (node->balance > 1) {
#       ifdef _DEBUG
        if (g_vlib_log && g_vlib_log->level >= LOG_LVL_DEBUG)
            avltree_print(tree, avltree_print_node_default, stderr);
#       endif
        // left rotate
        if (node->right->balance < 0) {
            //double rotation
            LOG_DEBUG(g_vlib_log, "right(%ld) left(%ld) rotation",
                                  (long)node->right->data, (long) node->data);
            *pparent = avltree_rotate_rightleft(tree, node);
        } else {
            LOG_DEBUG(g_vlib_log, "left(%ld) rotation", (long) node->data);
            *pparent = avltree_rotate_left(tree, node, 1);
        }
        idata->prev_child = *pparent;

        return AVS_FINISHED;
    } else {
        idata->prev_child = node;
    }

    if (idata->new_balance == 0) {
        return AVS_FINISHED;
    }

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

    if (context->state == AVH_SUFFIX) {
        return avltree_visit_rebalance(tree, node, context, user_data);
    } else if (context->state != AVH_PREFIX) {
        return AVS_ERROR;
    }

    LOG_DEBUG(g_vlib_log, "INSERTING %ld, Visiting Node %ld(%ld,%ld) How %d Ptr %lx(%lx,%lx)",
              (long)idata->newdata, (long)node->data,
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
    idata->prev_child = new;
    idata->new_balance = 1;

    LOG_DEBUG(g_vlib_log, "INSERTED node %ld(%lx,%lx) ptr 0x%lx on %s of %ld ptr 0x%lx",
              (long)new->data, (unsigned long)new->left, (unsigned long)new->right,
              (unsigned long) new,
              *parent == node->left ? "LEFT" : "RIGHT",
              (long)node->data, (unsigned long) node);

    /* stop prefix visit and switch to suffix visit */
    return AVS_NEXTVISIT;
}

/*****************************************************************************/
static int          avltree_visit_free(
                        avltree_t *                     tree,
                        avltree_node_t *                node,
                        const avltree_visit_context_t * context,
                        void *                          user_data) {
    (void) context;
    (void) user_data;

    avltree_node_free(tree, node);
    return AVS_CONTINUE;
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
    rbuf_t *    fifo;
    int         width       = 200;
    int         node_nb     = 1;
    int         node_sz     = width / 3;
    int         display     = (node_nb - 1) * node_sz;
    int         indent      = (width - display) / 2;
    ssize_t     n;
    ssize_t     old_idx     = -1;

    if ((tree->flags & AFL_SHARED_STACK) != 0) {
        fifo = tree->stack;
    } else {
        fifo = rbuf_create(32 * 3, RBF_DEFAULT);
    }

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

    if ((tree->flags & AFL_SHARED_STACK) != 0) {
        rbuf_reset(fifo);
    } else {
        rbuf_free(fifo);
    }
}

