#include <stdint.h>

#include "blitzmax/embedded_events.h"
#include "blitzmax/embedded_gpio.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BMX_ESP8266_GPIO_COUNT 17u
#define BMX_GPIO_IRQ_LEVEL_LOW 1u
#define BMX_GPIO_IRQ_LEVEL_HIGH 2u
#define BMX_GPIO_IRQ_EDGE_FALL 4u
#define BMX_GPIO_IRQ_EDGE_RISE 8u

static uint8_t bmx_esp8266_gpio_direction[BMX_ESP8266_GPIO_COUNT];
static uint8_t bmx_esp8266_gpio_output[BMX_ESP8266_GPIO_COUNT];
static uint8_t bmx_esp8266_gpio_pull_up[BMX_ESP8266_GPIO_COUNT];
static uint8_t bmx_esp8266_gpio_pull_down[BMX_ESP8266_GPIO_COUNT];
static volatile uint32_t bmx_esp8266_gpio_irq_events[BMX_ESP8266_GPIO_COUNT];
static volatile uint32_t bmx_esp8266_gpio_event_tokens[BMX_ESP8266_GPIO_COUNT];
static volatile uint32_t bmx_esp8266_gpio_irq_masks[BMX_ESP8266_GPIO_COUNT];
static uint8_t bmx_esp8266_gpio_isr_ready;

int32_t bmx_embedded_gpio_is_valid(uint32_t gpio) {
    return gpio < BMX_ESP8266_GPIO_COUNT && (gpio < 6u || gpio > 11u);
}

int32_t bmx_embedded_gpio_is_output_capable(uint32_t gpio) {
    return bmx_embedded_gpio_is_valid(gpio);
}

int32_t bmx_embedded_gpio_is_pull_capable(uint32_t gpio) {
    return bmx_embedded_gpio_is_valid(gpio);
}

int32_t bmx_embedded_gpio_is_pull_up_capable(uint32_t gpio) {
    return bmx_embedded_gpio_is_valid(gpio) && gpio != 16u;
}

int32_t bmx_embedded_gpio_is_pull_down_capable(uint32_t gpio) {
    return gpio == 16u;
}

int32_t bmx_embedded_gpio_is_drive_strength_configurable(uint32_t gpio) {
    (void)gpio;
    return 0;
}

int32_t bmx_embedded_gpio_is_irq_capable(uint32_t gpio) {
    return bmx_embedded_gpio_is_valid(gpio) && gpio != 16u;
}

int32_t bmx_embedded_gpio_init(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    gpio_config_t config = {
        .pin_bit_mask = 1u << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if (gpio_config(&config) != ESP_OK ||
            gpio_set_level((gpio_num_t)gpio, 0) != ESP_OK) return 0;
    bmx_esp8266_gpio_direction[gpio] = 0;
    bmx_esp8266_gpio_output[gpio] = 0;
    bmx_esp8266_gpio_pull_up[gpio] = 0;
    bmx_esp8266_gpio_pull_down[gpio] = 0;
    bmx_esp8266_gpio_irq_events[gpio] = 0;
    bmx_esp8266_gpio_event_tokens[gpio] = 0;
    bmx_esp8266_gpio_irq_masks[gpio] = 0;
    return 1;
}

int32_t bmx_embedded_gpio_set_direction(uint32_t gpio, int32_t direction) {
    if (!bmx_embedded_gpio_is_valid(gpio) || (direction != 0 && direction != 1)) return 0;
    if (gpio_set_direction((gpio_num_t)gpio,
            direction ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT) != ESP_OK) return 0;
    bmx_esp8266_gpio_direction[gpio] = direction != 0;
    return 1;
}

int32_t bmx_embedded_gpio_get_direction(uint32_t gpio) {
    return bmx_embedded_gpio_is_valid(gpio) && bmx_esp8266_gpio_direction[gpio];
}

int32_t bmx_embedded_gpio_set_input(uint32_t gpio) {
    return bmx_embedded_gpio_set_direction(gpio, 0);
}

int32_t bmx_embedded_gpio_set_output(uint32_t gpio) {
    return bmx_embedded_gpio_set_direction(gpio, 1);
}

int32_t bmx_embedded_gpio_get(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    return gpio_get_level((gpio_num_t)gpio) != 0;
}

int32_t bmx_embedded_gpio_put(uint32_t gpio, int32_t value) {
    if (!bmx_embedded_gpio_is_valid(gpio) ||
            gpio_set_level((gpio_num_t)gpio, value != 0) != ESP_OK) return 0;
    bmx_esp8266_gpio_output[gpio] = value != 0;
    return 1;
}

int32_t bmx_embedded_gpio_get_output(uint32_t gpio) {
    return bmx_embedded_gpio_is_valid(gpio) && bmx_esp8266_gpio_output[gpio];
}

int32_t bmx_embedded_gpio_set_pulls(uint32_t gpio, int32_t pull_up, int32_t pull_down) {
    if (!bmx_embedded_gpio_is_valid(gpio) || (pull_up && pull_down)) return 0;
    if ((pull_up && !bmx_embedded_gpio_is_pull_up_capable(gpio)) ||
            (pull_down && !bmx_embedded_gpio_is_pull_down_capable(gpio))) return 0;
    gpio_pull_mode_t mode = GPIO_FLOATING;
    if (pull_up) mode = GPIO_PULLUP_ONLY;
    if (pull_down) mode = GPIO_PULLDOWN_ONLY;
    if (gpio_set_pull_mode((gpio_num_t)gpio, mode) != ESP_OK) return 0;
    bmx_esp8266_gpio_pull_up[gpio] = pull_up != 0;
    bmx_esp8266_gpio_pull_down[gpio] = pull_down != 0;
    return 1;
}

int32_t bmx_embedded_gpio_pull_up(uint32_t gpio) {
    return bmx_embedded_gpio_set_pulls(gpio, 1, 0);
}

int32_t bmx_embedded_gpio_pull_down(uint32_t gpio) {
    return bmx_embedded_gpio_set_pulls(gpio, 0, 1);
}

int32_t bmx_embedded_gpio_disable_pulls(uint32_t gpio) {
    return bmx_embedded_gpio_set_pulls(gpio, 0, 0);
}

int32_t bmx_embedded_gpio_is_pulled_up(uint32_t gpio) {
    return bmx_embedded_gpio_is_valid(gpio) && bmx_esp8266_gpio_pull_up[gpio];
}

int32_t bmx_embedded_gpio_is_pulled_down(uint32_t gpio) {
    return bmx_embedded_gpio_is_valid(gpio) && bmx_esp8266_gpio_pull_down[gpio];
}

int32_t bmx_embedded_gpio_set_drive_strength(uint32_t gpio, int32_t drive_strength) {
    (void)gpio;
    (void)drive_strength;
    return 0;
}

int32_t bmx_embedded_gpio_get_drive_strength(uint32_t gpio) {
    (void)gpio;
    return 0;
}

static gpio_int_type_t bmx_esp8266_gpio_interrupt_type(uint32_t mask) {
    if (mask == BMX_GPIO_IRQ_LEVEL_LOW) return GPIO_INTR_LOW_LEVEL;
    if (mask == BMX_GPIO_IRQ_LEVEL_HIGH) return GPIO_INTR_HIGH_LEVEL;
    if (mask == BMX_GPIO_IRQ_EDGE_FALL) return GPIO_INTR_NEGEDGE;
    if (mask == BMX_GPIO_IRQ_EDGE_RISE) return GPIO_INTR_POSEDGE;
    if (mask == (BMX_GPIO_IRQ_EDGE_FALL | BMX_GPIO_IRQ_EDGE_RISE)) return GPIO_INTR_ANYEDGE;
    return GPIO_INTR_MAX;
}

static void bmx_esp8266_gpio_interrupt(void *context) {
    uint32_t gpio = (uint32_t)(uintptr_t)context;
    if (gpio >= 16u) return;
    uint32_t mask = bmx_esp8266_gpio_irq_masks[gpio];
    uint32_t events = mask;
    if (mask == (BMX_GPIO_IRQ_EDGE_FALL | BMX_GPIO_IRQ_EDGE_RISE)) {
        events = gpio_get_level((gpio_num_t)gpio) ? BMX_GPIO_IRQ_EDGE_RISE : BMX_GPIO_IRQ_EDGE_FALL;
    }
    uint64_t captured_time = (uint64_t)esp_timer_get_time();
    UBaseType_t state = taskENTER_CRITICAL_FROM_ISR();
    bmx_esp8266_gpio_irq_events[gpio] |= events;
    uint32_t token = bmx_esp8266_gpio_event_tokens[gpio];
    taskEXIT_CRITICAL_FROM_ISR(state);
    (void)bmx_embedded_event_post_from_isr_ex(token, events, gpio,
        (uint32_t)captured_time, (uint32_t)(captured_time >> 32u));
}

int32_t bmx_embedded_gpio_set_irq_enabled(uint32_t gpio, uint32_t mask, int32_t enabled) {
    if (!bmx_embedded_gpio_is_irq_capable(gpio) ||
            bmx_esp8266_gpio_interrupt_type(mask) == GPIO_INTR_MAX) return 0;
    if (!enabled) {
        if (gpio_set_intr_type((gpio_num_t)gpio, GPIO_INTR_DISABLE) != ESP_OK) return 0;
        if (bmx_esp8266_gpio_isr_ready) (void)gpio_isr_handler_remove((gpio_num_t)gpio);
        taskENTER_CRITICAL();
        bmx_esp8266_gpio_irq_masks[gpio] = 0;
        taskEXIT_CRITICAL();
        return 1;
    }
    if (!bmx_esp8266_gpio_isr_ready) {
        if (gpio_install_isr_service(0) != ESP_OK) return 0;
        bmx_esp8266_gpio_isr_ready = 1;
    }
    if (gpio_set_intr_type((gpio_num_t)gpio,
            bmx_esp8266_gpio_interrupt_type(mask)) != ESP_OK) return 0;
    taskENTER_CRITICAL();
    bmx_esp8266_gpio_irq_masks[gpio] = mask;
    taskEXIT_CRITICAL();
    if (gpio_isr_handler_add((gpio_num_t)gpio, bmx_esp8266_gpio_interrupt,
            (void *)(uintptr_t)gpio) == ESP_OK) return 1;
    taskENTER_CRITICAL();
    bmx_esp8266_gpio_irq_masks[gpio] = 0;
    taskEXIT_CRITICAL();
    return 0;
}

int32_t bmx_embedded_gpio_set_event_token(uint32_t gpio, uint32_t token) {
    if (!bmx_embedded_gpio_is_irq_capable(gpio)) return 0;
    taskENTER_CRITICAL();
    if (token && bmx_esp8266_gpio_event_tokens[gpio] &&
            bmx_esp8266_gpio_event_tokens[gpio] != token) {
        taskEXIT_CRITICAL();
        return 0;
    }
    bmx_esp8266_gpio_event_tokens[gpio] = token;
    taskEXIT_CRITICAL();
    return 1;
}

uint32_t bmx_embedded_gpio_pending_irq_events(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    taskENTER_CRITICAL();
    uint32_t events = bmx_esp8266_gpio_irq_events[gpio];
    taskEXIT_CRITICAL();
    return events;
}

uint32_t bmx_embedded_gpio_take_irq_events(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    taskENTER_CRITICAL();
    uint32_t events = bmx_esp8266_gpio_irq_events[gpio];
    bmx_esp8266_gpio_irq_events[gpio] = 0;
    taskEXIT_CRITICAL();
    return events;
}
