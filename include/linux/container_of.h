/// Stub implementation of Linux's container_of.h and compiler.h

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef __always_inline
#define __always_inline inline __attribute__((always_inline))
#endif

#define container_of(ptr, type, member) ((type*) ((uint8_t*)(ptr) - offsetof(type, member)))
