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

    eevdf_queue_t* current_queue;
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
 * Add a new thread into the queue.
 *
 * @param queue         [IN] The queue to add to
 * @param node          [IN] The thread that we are adding
 */
void eevdf_queue_add(eevdf_queue_t* queue, eevdf_node_t* node);

/**
 * Schedule a node to run, will properly handle the current, if the last tick
 * returned NULL then this function should not be called until another thread
 * either wakes up or gets added.
 *
 * @param queue         [IN] The queue we are scheduling on
 * @param time_slice    [IN] The time since the last time schedule was called
 */
eevdf_node_t* eevdf_queue_schedule(eevdf_queue_t* queue, int64_t time_slice);
