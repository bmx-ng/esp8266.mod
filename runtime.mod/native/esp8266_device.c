#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_device.h"
#include "blitzmax/embedded_time.h"
#include "esp_clk.h"
#include "esp_system.h"

#define BMX_ESP8266_DEVICE_ID_BYTES 6u

static int32_t bmx_esp8266_device_id(
        uint8_t identifier[BMX_ESP8266_DEVICE_ID_BYTES]) {
    return esp_efuse_mac_get_default(identifier) == ESP_OK;
}

const BMXEmbeddedString *bmx_embedded_unique_device_id(void) {
    static const char hex[] = "0123456789ABCDEF";
    uint8_t identifier[BMX_ESP8266_DEVICE_ID_BYTES];
    char text[BMX_ESP8266_DEVICE_ID_BYTES * 2u];
    if (!bmx_esp8266_device_id(identifier))
        return bmx_embedded_string_from_ascii("", 0);
    for (uint32_t index = 0; index < BMX_ESP8266_DEVICE_ID_BYTES; ++index) {
        text[index * 2u] = hex[identifier[index] >> 4];
        text[index * 2u + 1u] = hex[identifier[index] & 15u];
    }
    return bmx_embedded_string_from_ascii(text, sizeof(text));
}

BMXEmbeddedArray *bmx_embedded_unique_device_id_bytes(void) {
    uint8_t identifier[BMX_ESP8266_DEVICE_ID_BYTES];
    if (!bmx_esp8266_device_id(identifier)) return &bmx_embedded_empty_array;
    BMXEmbeddedArray *result = bmx_embedded_array_new_1d(
        BMX_ESP8266_DEVICE_ID_BYTES, sizeof(uint8_t),
        BMX_EMBEDDED_ARRAY_ELEMENT_VALUE, NULL, NULL);
    if (result != &bmx_embedded_empty_array)
        memcpy(bmx_embedded_array_data(result), identifier, sizeof(identifier));
    return result;
}

int32_t bmx_esp8266_device_reset_reason_native(void) {
    return (int32_t)esp_reset_reason();
}

int32_t bmx_embedded_device_reset_reason(void) {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return BMX_EMBEDDED_RESET_REASON_POWER_ON;
        case ESP_RST_EXT: return BMX_EMBEDDED_RESET_REASON_EXTERNAL;
        case ESP_RST_SW:
        case ESP_RST_FAST_SW: return BMX_EMBEDDED_RESET_REASON_SOFTWARE;
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT: return BMX_EMBEDDED_RESET_REASON_WATCHDOG;
        case ESP_RST_PANIC: return BMX_EMBEDDED_RESET_REASON_PANIC;
        case ESP_RST_DEEPSLEEP: return BMX_EMBEDDED_RESET_REASON_DEEP_SLEEP;
        case ESP_RST_BROWNOUT: return BMX_EMBEDDED_RESET_REASON_BROWNOUT;
        default: return BMX_EMBEDDED_RESET_REASON_UNKNOWN;
    }
}

int32_t bmx_embedded_device_reboot(uint32_t delay_ms) {
    if (delay_ms) bmx_embedded_sleep_milliseconds(delay_ms);
    esp_restart();
}

static esp_chip_info_t bmx_esp8266_chip_info(void) {
    esp_chip_info_t info;
    esp_chip_info(&info);
    return info;
}

const BMXEmbeddedString *bmx_esp8266_device_chip_model_name(void) {
    static const char name[] = "ESP8266";
    return bmx_embedded_string_from_ascii(name, sizeof(name) - 1u);
}

uint32_t bmx_esp8266_device_chip_revision(void) {
    return bmx_esp8266_chip_info().revision;
}

uint32_t bmx_esp8266_device_chip_cores(void) {
    return bmx_esp8266_chip_info().cores;
}

uint32_t bmx_esp8266_device_chip_features(void) {
    return bmx_esp8266_chip_info().features;
}

uint32_t bmx_esp8266_device_cpu_frequency_hz(void) {
    int frequency = esp_clk_cpu_freq();
    return frequency > 0 ? (uint32_t)frequency : 0u;
}

uint32_t bmx_esp8266_device_free_heap(void) {
    return esp_get_free_heap_size();
}

uint32_t bmx_esp8266_device_minimum_free_heap(void) {
    return esp_get_minimum_free_heap_size();
}

int32_t bmx_esp8266_device_flash_size_map(void) {
    return (int32_t)system_get_flash_size_map();
}
