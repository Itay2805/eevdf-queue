#include <eevdf-queue/platform.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

noreturn void eevdf_platform_assert_failed(const char* msg) {
    fprintf(stderr, "eevdf assertion failed: %s\n", msg);
    abort();
}
