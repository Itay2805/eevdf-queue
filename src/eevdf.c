#include <eevdf-queue/eevdf.h>

#include <stddef.h>

_Static_assert(offsetof(eevdf_node_t, node) == 0, "The node in eevdf_node must be first");

/**
 * The weights that are used for each priority level
 */
static uint8_t m_eevdf_priority_weight[EEVDF_PRIORITY_MAX] = {
    [EEVDF_PRIORITY_LOWEST] = 1,
    [EEVDF_PRIORITY_BELOW_NORMAL] = 2,
    [EEVDF_PRIORITY_NORMAL] = 3,
    [EEVDF_PRIORITY_ABOVE_NORMAL] = 4,
    [EEVDF_PRIORITY_HIGHEST] = 5,
};

/**
 * Get the lag of a node, assuming we are given the node's queue
 */
static int64_t eevdf_queue_get_lag(eevdf_queue_t* queue, eevdf_node_t* node) {
    return (queue->total_ideal_runtime[node->priority] - node->ideal_runtime_base) - node->runtime;
}

/**
 * Calculate the deadline of the thread
 */
static uint64_t eevdf_queue_calculate_virtual_deadline(eevdf_queue_t* queue, eevdf_node_t* node) {
    return queue->virtual_time + ((node->time_slice * queue->weights_sum) / m_eevdf_priority_weight[node->priority]);
}

static bool eevdf_heap_deadline_less_than(heap_node_t* _a, heap_node_t* _b) {
    eevdf_node_t* a = (eevdf_node_t*)_a;
    eevdf_node_t* b = (eevdf_node_t*)_b;
    return a->virtual_deadline < b->virtual_deadline;
}

static bool eevdf_heap_lag_bigger_than(heap_node_t* _a, heap_node_t* _b) {
    eevdf_node_t* a = (eevdf_node_t*)_a;
    eevdf_node_t* b = (eevdf_node_t*)_b;
    eevdf_queue_t* queue = a->current_queue;
    return eevdf_queue_get_lag(queue, a) > eevdf_queue_get_lag(queue, b);
}

/**
 * Insert the node to the eligible queue
 */
static void eevdf_queue_insert_eligible(eevdf_queue_t* queue, eevdf_node_t* node) {
    heap_insert(&queue->eligible, &node->node, eevdf_heap_deadline_less_than);
}

/**
 * Insert the node to the correct decaying loop
 */
static void eevdf_queue_insert_decaying(eevdf_queue_t* queue, eevdf_node_t* node) {
    heap_insert(&queue->decaying[node->priority], &node->node, eevdf_heap_lag_bigger_than);
}

/**
 * Takes a node and makes it eligible, calculating the correct deadline
 * for it before inserting into the eligible queue
 */
static void eevdf_queue_make_eligible(eevdf_queue_t* queue, eevdf_node_t* node) {
    // Reset the ideal runtime base, the runtime value will have the
    // lag calculated into it already, making sure that we get the same
    // lag that we had beforehand
    node->ideal_runtime_base = queue->total_ideal_runtime[node->priority];

    // update the deadline again
    node->virtual_deadline = eevdf_queue_calculate_virtual_deadline(queue, node);

    // Insert to the eligible queue
    eevdf_queue_insert_eligible(queue, node);
}

void eevdf_queue_add(eevdf_queue_t* queue, eevdf_node_t* node) {
    // set the node
    node->current_queue = queue;

    // update the total weights we have
    queue->weights_sum += m_eevdf_priority_weight[node->priority];

    // reset state just in case
    node->remove = false;
    node->runtime = 0;

    // And now make it eligible
    eevdf_queue_make_eligible(queue, node);
}

void eevdf_node_wakeup(eevdf_node_t* node) {
    // we requeue on the queue it was last on
    eevdf_queue_t* queue = node->current_queue;

    // TODO: lock the queue in case of remote wakeup

    if (node->remove) {
        // the node was still not removed from the
        // decaying list, so keep it there
        node->remove = false;

        // add back the weight now that we don't want it removed
        node->current_queue += m_eevdf_priority_weight[node->priority];

    } else {
        // we can only wakeup if the task was removed
        eevdf_queue_make_eligible(queue, node);
    }
}

/**
 * Check if any of the decaying nodes need to be removed form the decaying list
 * either to the eligible list or completely from the lists
 */
static void eevdf_queue_update_decaying(eevdf_queue_t* queue) {
    for (int i = 0; i < EEVDF_PRIORITY_MAX; i++) {
        heap_t* heap = &queue->decaying[i];

        // iterate the min nodes
        for (heap_node_t* node = heap_min_node(heap); node != NULL; node = heap_min_node(heap)) {
            // check if the node still has negative lag
            eevdf_node_t* enode = (eevdf_node_t*)node;
            int64_t lag = eevdf_queue_get_lag(queue, enode);
            if (lag < 0) {
                break;
            }

            // remove from the heap
            heap_pop(heap, eevdf_heap_lag_bigger_than);

            // and now check what we need to do with the node
            if (enode->remove) {
                // the node is still not queued, reset its runtime
                // since it finished decaying
                enode->runtime = 0;

                // and mark that it does not need to be removed anymore
                enode->remove = false;
            } else {
                // the node is ready to be requeued, update
                // the deadline
                enode->virtual_deadline = eevdf_queue_calculate_virtual_deadline(queue, enode);

                // and queue on the eligible queue
                eevdf_queue_insert_eligible(queue, enode);
            }
        }
    }
}

static void eevdf_queue_tick(eevdf_queue_t* queue, int64_t time_slice) {
    // update the lag of each since last boot
    for (int i = 0; i < EEVDF_PRIORITY_MAX; i++) {
        queue->total_ideal_runtime[i] += (i * time_slice) / queue->weights_sum;
    }

    // update the virtual time of the queue
    queue->virtual_time += time_slice / queue->weights_sum;

    // update the decaying entries, remove anything if need be
    eevdf_queue_update_decaying(queue);
}

static void eevdf_queue_tick_current(eevdf_queue_t* queue, int64_t time_slice, bool requeue) {
    eevdf_node_t* current = queue->current;

    // make sure we have a current (in case we woke up from sleep)
    if (current == NULL) {
        return;
    }

    // update its runtime
    current->runtime += time_slice;

    int64_t lag = eevdf_queue_get_lag(queue, current);
    if (lag < 0) {
        // this now has a negative lag, so we need to
        // remove it from the queue

        if (!requeue) {
            // we don't want this to be requeud, mark for removal once
            // the decay is complete
            current->remove = true;

            // remove the weight
            queue->weights_sum -= m_eevdf_priority_weight[current->priority];
        }

        // Insert to the decaying loop until we are done
        eevdf_queue_insert_decaying(queue, current);

    } else if (requeue) {
        // update the deadline now that it is back to being eligible
        current->virtual_deadline = eevdf_queue_calculate_virtual_deadline(queue, current);

        // put back on the eligible queue
        eevdf_queue_insert_eligible(queue, current);

    } else {
        // This should not be re-queued, set the runtime to -lag
        // so that on next wakeup it will get the same lag as it
        // has right now
        current->runtime = -lag;
    }
}

static eevdf_node_t* eevdf_queue_choose_next(eevdf_queue_t* queue) {
    return (eevdf_node_t*)heap_pop(&queue->eligible, eevdf_heap_deadline_less_than);
}

eevdf_node_t* eevdf_queue_schedule(eevdf_queue_t* queue, int64_t time_slice, bool remove, bool requeue) {
    // if there are no weights then there are no threads
    // so just return
    if (queue->weights_sum == 0) {
        return NULL;
    }

    // tick the queue
    eevdf_queue_tick(queue, time_slice);

    // tick the current task if any
    if (!remove) {
        eevdf_queue_tick_current(queue, time_slice, requeue);
    }

    // and now choose the next node to run
    eevdf_node_t* node = eevdf_queue_choose_next(queue);
    queue->current = node;
    return node;
}
