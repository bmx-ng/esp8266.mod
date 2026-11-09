#ifndef BLITZMAX_EMBEDDED_PLATFORM_H
#define BLITZMAX_EMBEDDED_PLATFORM_H

#include <stdio.h>
#include <stdlib.h>

/* The ESP8266 GCC 8/newlib headers predate C11 max_align_t. The precise
   runtime uses a fixed 16-byte allocation contract on every embedded target. */
#if !defined(__CLANG_MAX_ALIGN_T_DEFINED) && !defined(_GCC_MAX_ALIGN_T)
typedef struct __attribute__((aligned(16))) BMXESP8266MaxAlign {
    unsigned char bytes[16];
} max_align_t;
#endif

void *bmx_esp8266_managed_arena_acquire(uint32_t capacity, uint32_t alignment);

/* ESP8266 currently runs managed BlitzMax code on the app_main FreeRTOS task.
   Native SDK callbacks must hand plain data back to that task before entering
   managed code. */
#define BMX_EMBEDDED_PLATFORM_CONTEXT_VALID() 1
#define BMX_EMBEDDED_PLATFORM_CONTEXT_INDEX() 0u
#define BMX_EMBEDDED_PLATFORM_PANIC(message) do { \
    fputs((message), stderr); \
    fputc('\n', stderr); \
    abort(); \
} while (0)
#define BMX_EMBEDDED_PLATFORM_ARENA_ACQUIRE(capacity, alignment) \
    bmx_esp8266_managed_arena_acquire((capacity), (alignment))

#endif
