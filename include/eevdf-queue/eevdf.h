#pragma once

#include <linux/rbtree.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct eevdf_queue eevdf_queue_t;
typedef struct eevdf_node eevdf_node_t;

typedef struct eevdf_node {
    // the priority of the node
    uint8_t weight;

    // the time slice the node needs/wants (physical time)
    uint32_t time_slice;

    struct rb_node timeline_node;

    int64_t vdeadline;
    int64_t vruntime;
    int64_t min_vruntime;
} eevdf_node_t;

typedef struct eevdf_queue {
    int64_t vtime;
    uint32_t total_weight;

    struct rb_root_cached timeline;
    eevdf_node_t* current;
} eevdf_queue_t;

/**
 * Adds a new thread to the queue. This thread must not be attached to any other
 * queue.
 *
 * @param[in] queue The queue to add to
 * @param[in] node  The thread to add
 */
void eevdf_queue_add(eevdf_queue_t* queue, eevdf_node_t* node);

/**
 * Accounts timing information for the currently-executing thread (as returned
 * by the last call to this function) and selects a new thread to run.
 *
 * If this function returns null, there are currently no runnable threads, and
 * that situation will not change until new threads are added (i.e., the
 * function need not be called again until `eevdf_queue_add` has been called
 * to insert a new thread).
 *
 * If `requeue_curr` is true, the currently-executing thread (if there is
 * one) will be reinserted into the queue before a new thread is selected. This
 * is suitable for implementing preemption and similar yield operations, where
 * the preempted thread is still "ready" to run after being interrupted.
 *
 * If `requeue_curr` is false, the current thread will be completely removed
 * from the queue. This is suitable for implementing thread exit or parking routines.
 *
 * @param[in] queue        The queue to reschedule
 * @param[in] time_slice   The time elapsed since the last call to this function
 * @param[in] requeue_curr If true, the currently-executing thread will be
 *                         reinserted into the queue for selection; otherwise,
 *                         it will be removed
 */
eevdf_node_t* eevdf_queue_schedule(eevdf_queue_t* queue, int64_t time_slice,
                                   bool requeue_curr);
