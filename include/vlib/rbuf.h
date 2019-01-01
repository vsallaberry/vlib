/*
 * Copyright (C) 2018-2019 Vincent Sallaberry
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
 * Simple ring buffer utilities, supporting stack(lifo) & queue(fifo) features.
 *   rbuf_t * rbuf = rbuf_create(10, RBF_DEFAULT);
 *   rbuf_push(rbuf, (void*) 1l);
 *   rbuf_push(rbuf, (void*) 2l);
 *   while (rbuf_size(rbuf) != 0) printf(">%ld\n", (long)rbuf_pop(rbuf));
 *      // lifo: prints 2, 1
 *   rbuf_push(rbuf, (void*) 1l);
 *   rbuf_push(rbuf, (void*) 2l);
 *   while (rbuf_size(rbuf) != 0) printf(">%ld\n", (long)rbuf_dequeue(rbuf));
 *     // fifo: prints 1, 2
 *   rbuf_free(rbuf);
 */
#ifndef VLIB_RBUF_H
#define VLIB_RBUF_H

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/
#define VLIB_RBUF_SZ   64

typedef enum {
    RBF_NONE            = 0,
    RBF_OVERWRITE       = 1 << 0, /* when buffer is full, overwrite instead of increasing size */
    RBF_SHRINK_ON_RESET = 1 << 1, /* restore initial buffer size when calling rbuf_reset() */
    RBF_DEFAULT         = RBF_NONE,
} rbuf_flags_t;

/** opaque struct rbuf_t/rbuf_s */
typedef struct rbuf_s   rbuf_t;

/*****************************************************************************/

/** create a ring buffer of initial size <max_size>.
 * unless RBF_OVERWRITE is ON, max_size is doubled in rbuf_push() if buffer is full
 * @param max_size the maximum number of elements in the buffer, which can increase if
 *        RBF_OVERWRITE is not given
 * @param flags the buffer configuration @see rbuf_flags_t
 * @return the new created buffer or NULL on error */
rbuf_t *        rbuf_create(
                    size_t          max_size,
                    rbuf_flags_t    flags);

/** empty the buffer and restore its initial size if RBF_SHRINK_ON_RESET is given
 * @return 0 on success and -1 on error (bad paramaters, out of memory) */
int             rbuf_reset(
                    rbuf_t *        rbuf);

/** clean buffer data and release memory
 * @param rbuf the buffer */
void            rbuf_free(
                    rbuf_t *        rbuf);

/** number of elements in the buffer
 * @param rbuf the buffer
 * @return number of elements in the buffer
 *         or 0 on error with errno set (errno is NOT changed on success). */
size_t          rbuf_size(
                    const rbuf_t *  rbuf);

/** current maximum size for the buffer
 *  (note: it can increase if not created with RBF_OVERWRITE)
 * @param rbuf the buffer
 * @return current maximum size of buffer
 *         or 0 on error with errno set (errno is NOT changed on success). */
size_t          rbuf_maxsize(
                    const rbuf_t *  rbuf);

/** estimation of memory used by the buffer
 * @param rbuf the buffer
 * @return current estimation of memory used by the buffer
 *         or 0 on error with errno set (errno is NOT changed on success). */
size_t          rbuf_memorysize(
                    const rbuf_t *  rbuf);

/** push an element at end of the buffer: compatible with lifo(stack)/fifo(queue)
 * @param rbuf the buffer
 * @param data the element to be pushed
 * @return 0 on success, -1 if element cannot be pushed (bad parameters, out of memory). */
int             rbuf_push(
                    rbuf_t *        rbuf,
                    void *          data);

/** Last element. This one would be returned by rbuf_pop()
 * @return the last element or NULL on error with errno set (errno is NOT changed on success). */
void *          rbuf_top(
                    const rbuf_t *  rbuf);

/** remove and return last element of the buffer : LIFO(stack) mode.
 * @return the popped element or NULL with errno set (errno is NOT changed on success). */
void *          rbuf_pop(
                    rbuf_t *        rbuf);

/** First element. This one would be returned by rbuf_dequeue()
 * @return the first element or NULL with errno set (errno is NOT changed on success). */
void *          rbuf_bottom(
                    const rbuf_t *  rbuf);

/** remove and return first element of the buffer : FIFO(queue) mode.
 * @return the dequeued element or NULL with errno set (errno is NOT changed on success). */
void *          rbuf_dequeue(
                    rbuf_t *        rbuf);

/** get the index-th element of the buffer
 * @return the index-th element or NULL with errno set (errno is NOT changed on success). */
void *          rbuf_get(
                    const rbuf_t *  rbuf,
                    size_t          index);

/** set the index-th element in the buffer.
 * if RBF_OVERWRITE is OFF, the buffer is increased so that index will fit.
 * @return 0 on success or -1 on error */
int             rbuf_set(
                    rbuf_t *        rbuf,
                    size_t          index,
                    void *          data);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */


