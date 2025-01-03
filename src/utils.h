#pragma once

#include <eevdf-queue/platform.h>

#define ASSERT(cond, msg)                                                      \
    do {                                                                       \
        if (!(cond))                                                           \
            eevdf_platform_assert_failed(msg);                                 \
    } while (0)
