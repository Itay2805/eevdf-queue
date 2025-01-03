#include <eevdf-queue/eevdf.h>
#include <linux/container_of.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>

#define MAX_NODES 10

#define WEIGHT_MIN 1
#define WEIGHT_MAX 10

#define TIME_SLICE_MIN 5000
#define TIME_SLICE_MAX 50000

#define SCHED_ITERATIONS 20

typedef struct sched_node {
    eevdf_node_t eevdf_node;
    size_t index;
} sched_node_t;

static void init_rng(void) {
    uint32_t seed = 0;
    getrandom(&seed, sizeof(seed), 0);
    printf("seed: %#x\n", seed);
    srand(seed);
}

static int rand_range(int min, int max) {
    return rand() % (max - min) + min;
}

int main(void) {
    eevdf_queue_t queue = {0};

    init_rng();

    size_t n = rand_range(1, MAX_NODES);
    for (size_t i = 0; i < n; i++) {
        sched_node_t* node = calloc(1, sizeof(*node));
        node->index = i;
        node->eevdf_node.weight = rand_range(WEIGHT_MIN, WEIGHT_MAX);
        node->eevdf_node.time_slice =
            rand_range(TIME_SLICE_MIN, TIME_SLICE_MAX);
        printf("node %zu: weight %u, time slice %u\n", i,
               node->eevdf_node.weight, node->eevdf_node.time_slice);
        eevdf_queue_add(&queue, &node->eevdf_node);
    }

    uint32_t time_slice = 0;
    for (size_t i = 0; i < SCHED_ITERATIONS; i++) {
        eevdf_node_t* eevdf_node =
            eevdf_queue_schedule(&queue, time_slice, true);
        if (!eevdf_node) {
            printf("idle\n");
            break;
        }

        sched_node_t* node = container_of(eevdf_node, sched_node_t, eevdf_node);
        time_slice = node->eevdf_node.time_slice;
        printf("run %zu for %u\n", node->index, time_slice);
    }
}
