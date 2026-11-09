#include <stdint.h>

#include "blitzmax/embedded_adc.h"
#include "driver/adc.h"

#define BMX_ESP8266_ADC_EXTERNAL_PIN 17u

static int32_t bmx_esp8266_adc_active;

int32_t bmx_embedded_adc_is_valid_pin(uint32_t pin) {
    return pin == BMX_ESP8266_ADC_EXTERNAL_PIN;
}

int32_t bmx_embedded_adc_init_pin(uint32_t pin) {
    if (!bmx_embedded_adc_is_valid_pin(pin)) return 0;
    if (bmx_esp8266_adc_active) return 1;
    adc_config_t config = {
        .mode = ADC_READ_TOUT_MODE,
        .clk_div = 8,
    };
    if (adc_init(&config) != ESP_OK) return 0;
    bmx_esp8266_adc_active = 1;
    return 1;
}

int32_t bmx_embedded_adc_deinit_pin(uint32_t pin) {
    if (!bmx_embedded_adc_is_valid_pin(pin) || !bmx_esp8266_adc_active)
        return 0;
    if (adc_deinit() != ESP_OK) return 0;
    bmx_esp8266_adc_active = 0;
    return 1;
}

int32_t bmx_embedded_adc_read_raw(uint32_t pin, uint32_t *value) {
    uint16_t reading;
    if (!value || !bmx_embedded_adc_is_valid_pin(pin) ||
            !bmx_esp8266_adc_active || adc_read(&reading) != ESP_OK) return 0;
    *value = reading;
    return 1;
}

uint32_t bmx_embedded_adc_resolution_bits(uint32_t pin) {
    return bmx_embedded_adc_is_valid_pin(pin) ? 10u : 0u;
}

uint32_t bmx_embedded_adc_maximum_value(uint32_t pin) {
    return bmx_embedded_adc_is_valid_pin(pin) ? 1023u : 0u;
}

int32_t bmx_esp8266_adc_initialized(void) {
    return bmx_esp8266_adc_active;
}
