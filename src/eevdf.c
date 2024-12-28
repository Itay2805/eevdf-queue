#include <eevdf-queue/eevdf.h>
#include <linux/rbtree.h>
#include <linux/rbtree_augmented.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EEVDF_NODE_OF(node) rb_entry(node, eevdf_node_t, timeline_node)
#define EEVDF_NODE_OF_SAFE(node)                                               \
    rb_entry_safe(node, eevdf_node_t, timeline_node)

static inline void set_deadline(eevdf_node_t* node) {
    // The node will always become eligible exactly when its vruntime matches
    // the global vtime, so its eligible vtime is actually just its vruntime.
    node->vdeadline = node->vruntime + node->time_slice / node->weight;
}

static inline bool is_eligible(eevdf_node_t* node, int64_t vtime) {
    return node->vruntime <= vtime;
}

static inline bool subtree_has_eligible_node(eevdf_node_t* root,
                                             int64_t vtime) {
    return root->min_vruntime <= vtime;
}

static inline bool deadline_before(struct rb_node* lhs,
                                   const struct rb_node* rhs) {
    return EEVDF_NODE_OF(lhs)->vdeadline < EEVDF_NODE_OF(rhs)->vdeadline;
}

static bool update_min_vruntime(eevdf_node_t* node, bool exit) {
    int64_t min_vruntime = node->vruntime;

    if (node->timeline_node.rb_left) {
        int64_t left_min_vruntime =
            EEVDF_NODE_OF(node->timeline_node.rb_left)->min_vruntime;
        if (left_min_vruntime < min_vruntime) {
            min_vruntime = left_min_vruntime;
        }
    }

    if (node->timeline_node.rb_right) {
        int64_t right_min_vruntime =
            EEVDF_NODE_OF(node->timeline_node.rb_right)->min_vruntime;
        if (right_min_vruntime < min_vruntime) {
            min_vruntime = right_min_vruntime;
        }
    }

    bool done = min_vruntime == node->min_vruntime;
    node->min_vruntime = min_vruntime;

    return done;
}

RB_DECLARE_CALLBACKS(static, min_vruntime_callbacks, eevdf_node_t,
                     timeline_node, min_vruntime, update_min_vruntime);

static void enqueue_node(eevdf_queue_t* queue, eevdf_node_t* node) {
    rb_add_augmented_cached(&node->timeline_node, &queue->timeline,
                            deadline_before, &min_vruntime_callbacks);
}

static void dequeue_node(eevdf_queue_t* queue, eevdf_node_t* node) {
    rb_erase_augmented_cached(&node->timeline_node, &queue->timeline,
                              &min_vruntime_callbacks);
}

static eevdf_node_t* eevdf_queue_pick(eevdf_queue_t* queue) {
    // Optimization: we have easy access to the node with the earliest
    // deadline, and it will be the correct choice if it is eligible. Skip the
    // tree walk in that case.
    eevdf_node_t* earliest_deadline =
        EEVDF_NODE_OF(rb_first_cached(&queue->timeline));
    if (is_eligible(earliest_deadline, queue->vtime)) {
        return earliest_deadline;
    }

    // Slow case: walk down the tree, searching for the leftmost eligible node.
    // We can use the `min_vruntime` field to prune entire subtrees that are
    // ineligible.

    // Assumption: our tree always contains at least one eligible node (because
    // lags always sum to 0). We can then derive the loop invariant that `node`
    // always has at least one eligible node in its subtree.

    eevdf_node_t* node = EEVDF_NODE_OF(queue->timeline.rb_root.rb_node);
    while (true) {
        eevdf_node_t* left = EEVDF_NODE_OF_SAFE(node->timeline_node.rb_left);

        // If the node's left subtree has any eligible nodes, descend into it.
        if (left && subtree_has_eligible_node(left, queue->vtime)) {
            node = left;
            continue;
        }

        // `node` doesn't have any eligible left descendents. If it is eligible
        // itself, it is the leftmost eligible node.
        if (is_eligible(node, queue->vtime)) {
            break;
        }

        // Otherwise, all eligible nodes must reside in the right subtree (which
        // must necessarily exist by the loop invariant). Descend there now.
        node = EEVDF_NODE_OF(node->timeline_node.rb_right);
    }

    return node;
}

void eevdf_queue_add(eevdf_queue_t* queue, eevdf_node_t* node) {
    queue->total_weight += node->weight;

    // For now: always insert with a lag of 0.
    node->vruntime = queue->vtime;
    set_deadline(node);
    enqueue_node(queue, node);
}

eevdf_node_t* eevdf_queue_schedule(eevdf_queue_t* queue, int64_t time_slice) {
    if (!queue->total_weight) {
        // If nothing is currently running on this queue, our virtual clock is
        // paused and nothing can be selected for execution.
        return NULL;
    }

    queue->vtime += time_slice / queue->total_weight;

    eevdf_node_t* current = queue->current;
    if (current) {
        current->vruntime += time_slice / current->weight;
        set_deadline(current);
        enqueue_node(queue, current);
    }

    eevdf_node_t* next = eevdf_queue_pick(queue);
    dequeue_node(queue, next);
    queue->current = next;

    return next;
}
