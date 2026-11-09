#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_pwm.h"
#include "driver/gpio.h"
#include "driver/pwm.h"

#define BMX_ESP8266_PWM_CHANNELS 8u
#define BMX_ESP8266_PWM_CLOCK 1000000u
#define BMX_ESP8266_PWM_MINIMUM_PERIOD 20u

typedef struct BMXESP8266PWMChannel {
    uint32_t pin;
    uint32_t duty;
    uint8_t enabled;
    uint8_t inverted;
} BMXESP8266PWMChannel;

static BMXESP8266PWMChannel bmx_esp8266_pwm_channels[BMX_ESP8266_PWM_CHANNELS];
static uint32_t bmx_esp8266_pwm_frequency;
static uint32_t bmx_esp8266_pwm_period;
static uint8_t bmx_esp8266_pwm_count;
static uint8_t bmx_esp8266_pwm_started;

int32_t bmx_embedded_pwm_is_valid_pin(uint32_t pin) {
    return pin <= 16u && (pin < 6u || pin > 11u);
}

static void bmx_esp8266_pwm_reset_pin(uint32_t pin) {
    gpio_config_t config = {
        .pin_bit_mask = UINT64_C(1) << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&config);
}

static int32_t bmx_esp8266_pwm_channel(uint32_t pin) {
    for (uint8_t channel = 0; channel < bmx_esp8266_pwm_count; ++channel) {
        if (bmx_esp8266_pwm_channels[channel].pin == pin) return channel;
    }
    return -1;
}

static uint32_t bmx_esp8266_pwm_period_for_frequency(uint32_t frequency) {
    if (!frequency) return 0;
    uint32_t period = (BMX_ESP8266_PWM_CLOCK + frequency / 2u) / frequency;
    return period >= BMX_ESP8266_PWM_MINIMUM_PERIOD ? period : 0u;
}

static uint32_t bmx_esp8266_pwm_achieved_frequency(uint32_t period) {
    return period ? BMX_ESP8266_PWM_CLOCK / period : 0u;
}

static uint32_t bmx_esp8266_pwm_native_duty(
        const BMXESP8266PWMChannel *channel, uint32_t period) {
    if (!channel->enabled) return 0;
    return (uint32_t)(((uint64_t)channel->duty * period +
        BMX_EMBEDDED_PWM_DUTY_MAXIMUM / 2u) /
        BMX_EMBEDDED_PWM_DUTY_MAXIMUM);
}

static uint16_t bmx_esp8266_pwm_invert_mask(void) {
    uint16_t mask = 0;
    for (uint8_t channel = 0; channel < bmx_esp8266_pwm_count; ++channel) {
        if (bmx_esp8266_pwm_channels[channel].inverted)
            mask |= (uint16_t)(1u << channel);
    }
    return mask;
}

static int32_t bmx_esp8266_pwm_rebuild(uint32_t period) {
    if (bmx_esp8266_pwm_started) {
        if (pwm_deinit() != ESP_OK) return 0;
        bmx_esp8266_pwm_started = 0;
    }
    if (!bmx_esp8266_pwm_count) {
        bmx_esp8266_pwm_period = 0;
        bmx_esp8266_pwm_frequency = 0;
        return 1;
    }
    uint32_t pins[BMX_ESP8266_PWM_CHANNELS];
    uint32_t duties[BMX_ESP8266_PWM_CHANNELS];
    float phases[BMX_ESP8266_PWM_CHANNELS] = {0};
    for (uint8_t channel = 0; channel < bmx_esp8266_pwm_count; ++channel) {
        pins[channel] = bmx_esp8266_pwm_channels[channel].pin;
        duties[channel] = bmx_esp8266_pwm_native_duty(
            &bmx_esp8266_pwm_channels[channel], period);
    }
    if (pwm_init(period, duties, bmx_esp8266_pwm_count, pins) != ESP_OK)
        return 0;
    bmx_esp8266_pwm_started = 1;
    if (pwm_set_phases(phases) != ESP_OK ||
            pwm_set_channel_invert(bmx_esp8266_pwm_invert_mask()) != ESP_OK ||
            pwm_start() != ESP_OK) {
        (void)pwm_deinit();
        bmx_esp8266_pwm_started = 0;
        return 0;
    }
    bmx_esp8266_pwm_period = period;
    bmx_esp8266_pwm_frequency = bmx_esp8266_pwm_achieved_frequency(period);
    return 1;
}

static int32_t bmx_esp8266_pwm_apply(void) {
    uint32_t duties[BMX_ESP8266_PWM_CHANNELS];
    for (uint8_t channel = 0; channel < bmx_esp8266_pwm_count; ++channel) {
        duties[channel] = bmx_esp8266_pwm_native_duty(
            &bmx_esp8266_pwm_channels[channel], bmx_esp8266_pwm_period);
    }
    return pwm_set_duties(duties) == ESP_OK &&
        pwm_set_channel_invert(bmx_esp8266_pwm_invert_mask()) == ESP_OK &&
        pwm_start() == ESP_OK;
}

uint32_t bmx_embedded_pwm_init_pin(uint32_t pin, uint32_t frequency,
        uint32_t duty, int32_t inverted) {
    uint32_t period = bmx_esp8266_pwm_period_for_frequency(frequency);
    if (!bmx_embedded_pwm_is_valid_pin(pin) || !period ||
            duty > BMX_EMBEDDED_PWM_DUTY_MAXIMUM) return 0;
    int32_t existing = bmx_esp8266_pwm_channel(pin);
    if (existing >= 0) {
        if (bmx_esp8266_pwm_count > 1u &&
                bmx_esp8266_pwm_period != period) return 0;
        BMXESP8266PWMChannel previous = bmx_esp8266_pwm_channels[existing];
        bmx_esp8266_pwm_channels[existing].duty = duty;
        bmx_esp8266_pwm_channels[existing].enabled = 1;
        bmx_esp8266_pwm_channels[existing].inverted = inverted != 0;
        if (bmx_esp8266_pwm_period == period && bmx_esp8266_pwm_apply())
            return bmx_esp8266_pwm_frequency;
        if (bmx_esp8266_pwm_rebuild(period)) return bmx_esp8266_pwm_frequency;
        bmx_esp8266_pwm_channels[existing] = previous;
        (void)bmx_esp8266_pwm_rebuild(bmx_esp8266_pwm_period);
        return 0;
    }
    if (bmx_esp8266_pwm_count >= BMX_ESP8266_PWM_CHANNELS ||
            (bmx_esp8266_pwm_count && bmx_esp8266_pwm_period != period)) return 0;
    uint32_t previous_period = bmx_esp8266_pwm_period;
    uint8_t channel = bmx_esp8266_pwm_count++;
    bmx_esp8266_pwm_channels[channel].pin = pin;
    bmx_esp8266_pwm_channels[channel].duty = duty;
    bmx_esp8266_pwm_channels[channel].enabled = 1;
    bmx_esp8266_pwm_channels[channel].inverted = inverted != 0;
    if (bmx_esp8266_pwm_rebuild(period)) return bmx_esp8266_pwm_frequency;
    --bmx_esp8266_pwm_count;
    memset(&bmx_esp8266_pwm_channels[channel], 0,
        sizeof(bmx_esp8266_pwm_channels[channel]));
    if (bmx_esp8266_pwm_count)
        (void)bmx_esp8266_pwm_rebuild(previous_period);
    return 0;
}

int32_t bmx_embedded_pwm_deinit_pin(uint32_t pin) {
    int32_t channel = bmx_esp8266_pwm_channel(pin);
    if (channel < 0) return 0;
    BMXESP8266PWMChannel removed = bmx_esp8266_pwm_channels[channel];
    uint8_t old_count = bmx_esp8266_pwm_count;
    for (uint8_t index = (uint8_t)channel; index + 1u < old_count; ++index)
        bmx_esp8266_pwm_channels[index] = bmx_esp8266_pwm_channels[index + 1u];
    --bmx_esp8266_pwm_count;
    if (bmx_esp8266_pwm_rebuild(bmx_esp8266_pwm_period)) {
        bmx_esp8266_pwm_reset_pin(pin);
        return 1;
    }
    for (uint8_t index = bmx_esp8266_pwm_count; index > (uint8_t)channel; --index)
        bmx_esp8266_pwm_channels[index] = bmx_esp8266_pwm_channels[index - 1u];
    bmx_esp8266_pwm_channels[channel] = removed;
    bmx_esp8266_pwm_count = old_count;
    (void)bmx_esp8266_pwm_rebuild(bmx_esp8266_pwm_period);
    return 0;
}

uint32_t bmx_embedded_pwm_set_pin_frequency(uint32_t pin, uint32_t frequency) {
    int32_t channel = bmx_esp8266_pwm_channel(pin);
    uint32_t period = bmx_esp8266_pwm_period_for_frequency(frequency);
    if (channel < 0 || !period) return 0;
    if (period == bmx_esp8266_pwm_period) return bmx_esp8266_pwm_frequency;
    if (bmx_esp8266_pwm_count > 1u) return 0;
    uint32_t old_period = bmx_esp8266_pwm_period;
    if (bmx_esp8266_pwm_rebuild(period)) return bmx_esp8266_pwm_frequency;
    (void)bmx_esp8266_pwm_rebuild(old_period);
    return 0;
}

uint32_t bmx_embedded_pwm_get_pin_frequency(uint32_t pin) {
    return bmx_esp8266_pwm_channel(pin) >= 0 ? bmx_esp8266_pwm_frequency : 0u;
}

int32_t bmx_embedded_pwm_set_pin_duty(uint32_t pin, uint32_t duty) {
    int32_t channel = bmx_esp8266_pwm_channel(pin);
    if (channel < 0 || duty > BMX_EMBEDDED_PWM_DUTY_MAXIMUM) return 0;
    uint32_t previous = bmx_esp8266_pwm_channels[channel].duty;
    bmx_esp8266_pwm_channels[channel].duty = duty;
    if (bmx_esp8266_pwm_apply()) return 1;
    bmx_esp8266_pwm_channels[channel].duty = previous;
    (void)bmx_esp8266_pwm_apply();
    return 0;
}

uint32_t bmx_embedded_pwm_get_pin_duty(uint32_t pin) {
    int32_t channel = bmx_esp8266_pwm_channel(pin);
    return channel >= 0 ? bmx_esp8266_pwm_channels[channel].duty : 0u;
}

int32_t bmx_embedded_pwm_set_pin_polarity(uint32_t pin, int32_t inverted) {
    int32_t channel = bmx_esp8266_pwm_channel(pin);
    if (channel < 0) return 0;
    uint8_t previous = bmx_esp8266_pwm_channels[channel].inverted;
    bmx_esp8266_pwm_channels[channel].inverted = inverted != 0;
    if (bmx_esp8266_pwm_apply()) return 1;
    bmx_esp8266_pwm_channels[channel].inverted = previous;
    (void)bmx_esp8266_pwm_apply();
    return 0;
}

int32_t bmx_embedded_pwm_get_pin_polarity(uint32_t pin) {
    int32_t channel = bmx_esp8266_pwm_channel(pin);
    return channel >= 0 && bmx_esp8266_pwm_channels[channel].inverted;
}

int32_t bmx_embedded_pwm_set_pin_enabled(uint32_t pin, int32_t enabled) {
    int32_t channel = bmx_esp8266_pwm_channel(pin);
    if (channel < 0) return 0;
    uint8_t previous = bmx_esp8266_pwm_channels[channel].enabled;
    bmx_esp8266_pwm_channels[channel].enabled = enabled != 0;
    if (bmx_esp8266_pwm_apply()) return 1;
    bmx_esp8266_pwm_channels[channel].enabled = previous;
    (void)bmx_esp8266_pwm_apply();
    return 0;
}

int32_t bmx_embedded_pwm_get_pin_enabled(uint32_t pin) {
    int32_t channel = bmx_esp8266_pwm_channel(pin);
    return channel >= 0 && bmx_esp8266_pwm_channels[channel].enabled;
}

int32_t bmx_esp8266_pwm_channel_for_pin(uint32_t pin) {
    return bmx_esp8266_pwm_channel(pin);
}

uint32_t bmx_esp8266_pwm_active_channel_count(void) {
    return bmx_esp8266_pwm_count;
}
