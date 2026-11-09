#include <stdint.h>
#include <stdio.h>

#include "blitzmax/embedded_runtime.h"
#include "blitzmax/embedded_time.h"
#include "esp_clk.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

#ifndef BMX_EMBEDDED_ARENA_SIZE
#define BMX_EMBEDDED_ARENA_SIZE (12u * 1024u)
#endif

static _Alignas(16) uint8_t bmx_esp8266_managed_arena[BMX_EMBEDDED_ARENA_SIZE];

/* GCC emits this helper for relaxed 32-bit counter increments on LX106, but
   the ESP8266 toolchain does not ship libatomic. Serialize the operation with
   the target's interrupt-safe FreeRTOS critical section. */
uint32_t bmx_esp8266_atomic_fetch_add_4(volatile void *address,
    uint32_t value, int memory_order) __asm__("__atomic_fetch_add_4");
uint32_t bmx_esp8266_atomic_fetch_add_4(volatile void *address,
    uint32_t value, int memory_order) {
    (void)memory_order;
    taskENTER_CRITICAL();
    uint32_t previous = *(volatile uint32_t *)address;
    *(volatile uint32_t *)address = previous + value;
    taskEXIT_CRITICAL();
    return previous;
}

void *bmx_esp8266_managed_arena_acquire(uint32_t capacity, uint32_t alignment) {
    if (capacity != BMX_EMBEDDED_ARENA_SIZE || !alignment ||
            (alignment & (alignment - 1u)) || alignment > 16u) return NULL;
    return bmx_esp8266_managed_arena;
}

void __attribute__((noinline, used)) bmx_embedded_debug_stop(void) {
    __asm__ volatile ("break 0, 0" ::: "memory");
}

int32_t bmx_embedded_millisecs(void) {
    return (int32_t)(uint32_t)(esp_timer_get_time() / 1000);
}

void bmx_embedded_delay(int32_t milliseconds) {
    if (milliseconds <= 0) return;
    TickType_t ticks = pdMS_TO_TICKS((uint32_t)milliseconds);
    vTaskDelay(ticks ? ticks : 1);
}

void bmx_embedded_udelay(int32_t microseconds) {
    if (microseconds <= 0) return;
    while (microseconds > 0) {
        uint16_t chunk = microseconds > 65535 ? 65535u : (uint16_t)microseconds;
        os_delay_us(chunk);
        microseconds -= chunk;
    }
}

uint64_t bmx_embedded_time_microseconds(void) {
    return (uint64_t)esp_timer_get_time();
}

uint64_t bmx_embedded_time_milliseconds(void) {
    return bmx_embedded_time_microseconds() / 1000u;
}

uint32_t bmx_esp8266_system_clock_hz(void) {
    int frequency = esp_clk_cpu_freq();
    return frequency > 0 ? (uint32_t)frequency : 0u;
}

void bmx_embedded_sleep_microseconds(uint64_t microseconds) {
    while (microseconds) {
        if (microseconds >= 1000u) {
            uint32_t milliseconds = microseconds / 1000u > UINT32_MAX
                ? UINT32_MAX : (uint32_t)(microseconds / 1000u);
            bmx_embedded_delay((int32_t)(milliseconds > INT32_MAX ? INT32_MAX : milliseconds));
            microseconds -= (uint64_t)(milliseconds > INT32_MAX ? INT32_MAX : milliseconds) * 1000u;
        } else {
            bmx_embedded_udelay((int32_t)microseconds);
            microseconds = 0;
        }
    }
}

void bmx_embedded_sleep_milliseconds(uint32_t milliseconds) {
    while (milliseconds > INT32_MAX) {
        bmx_embedded_delay(INT32_MAX);
        milliseconds -= INT32_MAX;
    }
    bmx_embedded_delay((int32_t)milliseconds);
}

int32_t bmx_esp8266_stdio_init_all(void) {
    return 1;
}

int64_t bmx_esp8266_stdio_read(void *buffer, int64_t count) {
    if (!buffer || count <= 0) return 0;
    return (int64_t)fread(buffer, 1, (size_t)count, stdin);
}

int64_t bmx_esp8266_stdio_write(void *buffer, int64_t count) {
    if (!buffer || count <= 0) return 0;
    return (int64_t)fwrite(buffer, 1, (size_t)count, stdout);
}

void bmx_esp8266_stdio_flush(void) {
    fflush(stdout);
    fflush(stderr);
}
