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

#define SCHED_ITERATIONS 100

typedef struct sched_node {
    eevdf_node_t eevdf_node;
    size_t index;
    uint64_t total_runtime;
} sched_node_t;

static void init_rng(int argc, const char** argv) {
    uint32_t seed = 0;

    if (argc > 1) {
        const char* seed_arg = argv[1];
        char* seed_arg_end = NULL;
        seed = strtoul(seed_arg, &seed_arg_end, 0);
        if (!*seed_arg || *seed_arg_end) {
            puts("invalid seed argument");
            exit(1);
        }
    } else {
        getrandom(&seed, sizeof(seed), 0);
    }

    printf("seed: %#x\n", seed);
    srand(seed);
}

static int rand_range(int min, int max) {
    return rand() % (max - min) + min;
}

int main(int argc, const char** argv) {
    eevdf_queue_t queue = {0};

    init_rng(argc, argv);

    size_t n = rand_range(1, MAX_NODES);
    sched_node_t** nodes = calloc(n, sizeof(*nodes));

    uint32_t total_weight = 0;
    for (size_t i = 0; i < n; i++) {
        sched_node_t* node = calloc(1, sizeof(*node));
        node->index = i;
        node->eevdf_node.weight = rand_range(WEIGHT_MIN, WEIGHT_MAX);
        node->eevdf_node.time_slice =
            rand_range(TIME_SLICE_MIN, TIME_SLICE_MAX);

        printf("node %zu: weight %u, time slice %u\n", i,
               node->eevdf_node.weight, node->eevdf_node.time_slice);

        nodes[i] = node;
        total_weight += node->eevdf_node.weight;
        eevdf_queue_add(&queue, &node->eevdf_node);
    }

    uint64_t total_runtime = 0;
    sched_node_t* current = NULL;
    for (size_t i = 0; i < SCHED_ITERATIONS; i++) {
        uint32_t time_slice = 0;

        if (current) {
            time_slice = current->eevdf_node.time_slice;
            current->total_runtime += time_slice;
        }

        total_runtime += time_slice;

        eevdf_node_t* eevdf_node =
            eevdf_queue_schedule(&queue, time_slice, true);
        if (!eevdf_node) {
            printf("idle\n");
            break;
        }

        current = container_of(eevdf_node, sched_node_t, eevdf_node);
        printf("run %zu for %u\n", current->index, time_slice);
    }

    printf("total: runtime = %lu\n", total_runtime);
    for (size_t i = 0; i < n; i++) {
        uint64_t runtime = nodes[i]->total_runtime;
        uint8_t weight = nodes[i]->eevdf_node.weight;
        printf("node %zu: rel weight = %u%%, runtime = %lu (%lu%%)\n", i,
               (weight * 100) / total_weight, runtime,
               (100 * runtime) / total_runtime);
    }
}
