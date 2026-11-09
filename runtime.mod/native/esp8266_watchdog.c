#include <stdint.h>

#include "blitzmax/embedded_watchdog.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static TimerHandle_t bmx_esp8266_watchdog_timer;
static uint8_t bmx_esp8266_watchdog_active;
static uint8_t bmx_esp8266_watchdog_rebooted;
static RTC_NOINIT_ATTR uint32_t bmx_esp8266_watchdog_marker;

#define BMX_ESP8266_WATCHDOG_MARKER 0xb8266d07u

static void __attribute__((constructor)) bmx_esp8266_watchdog_startup(void) {
    if (bmx_esp8266_watchdog_marker == BMX_ESP8266_WATCHDOG_MARKER) {
        bmx_esp8266_watchdog_rebooted = 1;
        bmx_esp8266_watchdog_marker = 0;
    }
}

static void bmx_esp8266_watchdog_expired(TimerHandle_t timer) {
    (void)timer;
    /* Reprogramming the ESP8266 hardware watchdog for immediate reset leaves
       it armed across the warm reboot and can create a reset loop.  Retain a
       one-boot marker instead, then use the SDK's safe restart path. */
    bmx_esp8266_watchdog_marker = BMX_ESP8266_WATCHDOG_MARKER;
    esp_restart();
}

uint32_t bmx_embedded_watchdog_maximum_delay_ms(void) {
    uint64_t maximum = (uint64_t)(portMAX_DELAY - 1u) * portTICK_PERIOD_MS;
    return maximum > UINT32_MAX ? UINT32_MAX : (uint32_t)maximum;
}

int32_t bmx_embedded_watchdog_enable(uint32_t delay_ms) {
    uint32_t maximum = bmx_embedded_watchdog_maximum_delay_ms();
    if (!delay_ms || delay_ms > maximum) return 0;
    TickType_t ticks = (TickType_t)(((uint64_t)delay_ms + portTICK_PERIOD_MS - 1u) /
        portTICK_PERIOD_MS);
    if (!ticks) ticks = 1;
    if (!bmx_esp8266_watchdog_timer) {
        bmx_esp8266_watchdog_timer = xTimerCreate("bmx-wdt", ticks, pdFALSE,
            NULL, bmx_esp8266_watchdog_expired);
        if (!bmx_esp8266_watchdog_timer) return 0;
    }
    if (xTimerChangePeriod(bmx_esp8266_watchdog_timer, ticks, portMAX_DELAY) != pdPASS)
        return 0;
    bmx_esp8266_watchdog_active = 1;
    return 1;
}

int32_t bmx_embedded_watchdog_disable(void) {
    if (!bmx_esp8266_watchdog_active) return 1;
    if (xTimerStop(bmx_esp8266_watchdog_timer, portMAX_DELAY) != pdPASS) return 0;
    bmx_esp8266_watchdog_active = 0;
    return 1;
}

int32_t bmx_embedded_watchdog_feed(void) {
    if (!bmx_esp8266_watchdog_active || !bmx_esp8266_watchdog_timer) return 0;
    return xTimerReset(bmx_esp8266_watchdog_timer, portMAX_DELAY) == pdPASS;
}

int32_t bmx_embedded_watchdog_is_enabled(void) {
    return bmx_esp8266_watchdog_active && bmx_esp8266_watchdog_timer &&
        xTimerIsTimerActive(bmx_esp8266_watchdog_timer) != pdFALSE;
}

int32_t bmx_embedded_watchdog_caused_reboot(void) {
    esp_reset_reason_t reason = esp_reset_reason();
    return bmx_esp8266_watchdog_rebooted || reason == ESP_RST_TASK_WDT ||
        reason == ESP_RST_WDT;
}
