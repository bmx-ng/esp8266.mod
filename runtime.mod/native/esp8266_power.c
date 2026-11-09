#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum {
    BMX_POWER_CAPABILITY_IDLE = 0x01u,
    BMX_POWER_CAPABILITY_SLEEP_INTERRUPT = 0x02u,
    BMX_POWER_CAPABILITY_SLEEP_TIMER = 0x04u,
    BMX_POWER_UNAVAILABLE = -17,
    BMX_POWER_INVALID_ARGUMENT = -5,
    BMX_POWER_INVALID_STATE = -12,
    BMX_POWER_ERROR = -1
};

static int32_t bmx_esp8266_power_result(esp_err_t result) {
    if (result == ESP_OK) return 0;
    if (result == ESP_ERR_INVALID_ARG) return BMX_POWER_INVALID_ARGUMENT;
    if (result == ESP_ERR_INVALID_STATE) return BMX_POWER_INVALID_STATE;
    return BMX_POWER_ERROR;
}

static int32_t bmx_esp8266_power_light_sleep(uint32_t delay_us) {
    if (delay_us && esp_sleep_enable_timer_wakeup(delay_us) != ESP_OK)
        return BMX_POWER_ERROR;
    esp_err_t result = esp_light_sleep_start();
    if (delay_us)
        (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    return bmx_esp8266_power_result(result);
}

uint32_t bmx_embedded_power_capabilities(void) {
    return BMX_POWER_CAPABILITY_IDLE | BMX_POWER_CAPABILITY_SLEEP_INTERRUPT |
        BMX_POWER_CAPABILITY_SLEEP_TIMER;
}

void bmx_embedded_power_idle(void) {
    __asm__ volatile("waiti 0");
}

int32_t bmx_embedded_power_sleep_until_interrupt(void) {
    return bmx_esp8266_power_light_sleep(0);
}

int32_t bmx_embedded_power_sleep_for_ms(uint32_t milliseconds,
        int32_t exclusive) {
    (void)exclusive;
    if (!milliseconds || milliseconds > UINT32_MAX / 1000u)
        return BMX_POWER_INVALID_ARGUMENT;
    return bmx_esp8266_power_light_sleep(milliseconds * 1000u);
}

int32_t bmx_embedded_power_dormant_for_ms(uint32_t milliseconds) {
    (void)milliseconds;
    return BMX_POWER_UNAVAILABLE;
}

int32_t bmx_embedded_power_dormant_until_gpio(uint32_t pin, int32_t edge,
        int32_t high) {
    (void)pin;
    (void)edge;
    (void)high;
    return BMX_POWER_UNAVAILABLE;
}

int32_t bmx_embedded_power_set_unused_pins_low_leakage(uint64_t exclude_mask) {
    (void)exclude_mask;
    return BMX_POWER_UNAVAILABLE;
}

int32_t bmx_esp8266_power_sleep_until_gpio(uint32_t pin, int32_t high) {
    if (pin > 15u) return BMX_POWER_INVALID_ARGUMENT;
    gpio_int_type_t trigger = high ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
    esp_err_t result = gpio_wakeup_enable((gpio_num_t)pin, trigger);
    if (result != ESP_OK) return bmx_esp8266_power_result(result);
    result = esp_sleep_enable_gpio_wakeup();
    if (result == ESP_OK) result = esp_light_sleep_start();
    (void)gpio_wakeup_disable((gpio_num_t)pin);
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    return bmx_esp8266_power_result(result);
}

int32_t bmx_esp8266_power_deep_sleep_for_ms(uint64_t milliseconds) {
    if (!milliseconds || milliseconds > UINT64_MAX / 1000u)
        return BMX_POWER_INVALID_ARGUMENT;
    esp_deep_sleep(milliseconds * 1000u);
    return BMX_POWER_ERROR;
}

int32_t bmx_esp8266_power_deep_sleep_set_rf_option(int32_t option) {
    if (option != 0 && option != 1 && option != 2 && option != 4)
        return BMX_POWER_INVALID_ARGUMENT;
    esp_deep_sleep_set_rf_option((uint8_t)option);
    return 0;
}
