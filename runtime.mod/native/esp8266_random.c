#include <stddef.h>
#include <stdint.h>

#include "blitzmax/embedded_random.h"
#include "esp_system.h"

uint32_t bmx_embedded_random_uint32(void) {
    return esp_random();
}

uint64_t bmx_embedded_random_uint64(void) {
    uint64_t high = esp_random();
    return (high << 32u) | esp_random();
}

int32_t bmx_embedded_random_fill(void *buffer, int32_t length) {
    if (length < 0 || (length && !buffer)) return 0;
    if (length) esp_fill_random(buffer, (size_t)length);
    return 1;
}
