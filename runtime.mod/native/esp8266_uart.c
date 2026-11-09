#include <stdint.h>

#include "blitzmax/embedded_events.h"
#include "blitzmax/embedded_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp8266/pin_mux_register.h"
#include "esp8266/uart_register.h"
#include "esp_timer.h"

#define BMX_UART_INVALID_ARGUMENT (-5)
#define BMX_UART_ERROR_FRAMING 1u
#define BMX_UART_ERROR_PARITY 2u
#define BMX_UART_ERROR_OVERRUN 8u
#define BMX_UART_RX_BUFFER 256
#define BMX_UART_EVENT_CAPACITY 16

typedef struct BMXESP8266UARTState {
    QueueHandle_t event_queue;
    TimerHandle_t async_timer;
    uint8_t *async_rx_buffer;
    uint8_t *async_tx_buffer;
    uint32_t async_rx_capacity;
    uint32_t async_tx_capacity;
    volatile uint32_t async_rx_put;
    volatile uint32_t async_rx_get;
    volatile uint32_t async_tx_put;
    volatile uint32_t async_tx_get;
    uint32_t async_rx_dropped;
    uint32_t async_rx_token;
    uint32_t async_tx_token;
    uint32_t async_error_token;
    uint32_t errors;
    uint8_t configured;
    uint8_t initialized;
    uint8_t swapped;
    uint8_t flow_pins;
    uint8_t translate_crlf;
    uint8_t break_asserted;
    uint8_t async_open;
    volatile uint8_t async_tx_active;
    volatile int64_t async_tx_deadline;
} BMXESP8266UARTState;

static BMXESP8266UARTState bmx_esp8266_uart;
int32_t bmx_embedded_uart_set_break(int32_t controller, int32_t enabled);

static TickType_t bmx_esp8266_uart_ticks(uint32_t timeout_us) {
    if (!timeout_us) return 0;
    uint64_t milliseconds = ((uint64_t)timeout_us + 999u) / 1000u;
    TickType_t ticks = (TickType_t)((milliseconds + portTICK_PERIOD_MS - 1u) /
        portTICK_PERIOD_MS);
    return ticks ? ticks : 1;
}

static int32_t bmx_esp8266_uart_valid_buffer(const void *buffer, int32_t length) {
    return bmx_esp8266_uart.initialized && length >= 0 && (!length || buffer);
}

static int32_t bmx_esp8266_uart_wait_tx_done(void) {
    /* SDK 3.x uart_wait_tx_done() busy-spins after taking its completion
       semaphore.  Besides starving the idle task, that semaphore can refer
       to an earlier write when several chunks were submitted.  The portable
       adapter uses direct-FIFO mode, so a yielding hardware drain check is
       both sufficient and deterministic. */
    uint32_t baudrate = 0;
    if (uart_get_baudrate(UART_NUM_0, &baudrate) != ESP_OK || !baudrate) return 0;
    /* uart_write_bytes() in direct-FIFO mode does not return until every byte
       has entered the FIFO.  Allow one complete FIFO of worst-case 12-bit
       frames, plus two milliseconds for scheduler and shifter margin.  The
       FIFO count is deliberately not polled: on this chip it can remain
       asserted while UART0 is using the swapped GPIO15/GPIO13 route. */
    uint64_t microseconds = ((uint64_t)UART_FIFO_LEN * 12u * 1000000u +
        baudrate - 1u) / baudrate + 2000u;
    uint64_t tick_us = (uint64_t)portTICK_PERIOD_MS * 1000u;
    TickType_t ticks = (TickType_t)((microseconds + tick_us - 1u) / tick_us);
    if (!ticks) ticks = 1;
    vTaskDelay(ticks);
    return 1;
}

static void bmx_esp8266_uart_route_swapped(int32_t enabled) {
    portENTER_CRITICAL();
    if (enabled) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_UART0_CTS);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_UART0_RTS);
        SET_PERI_REG_MASK(UART_SWAP_REG, 0x4);
    } else {
        CLEAR_PERI_REG_MASK(UART_SWAP_REG, 0x4);
    }
    portEXIT_CRITICAL();
}

static uint32_t bmx_esp8266_uart_error_for_event(uart_event_type_t event) {
    if (event == UART_FRAME_ERR) return BMX_UART_ERROR_FRAMING;
    if (event == UART_PARITY_ERR) return BMX_UART_ERROR_PARITY;
    if (event == UART_FIFO_OVF || event == UART_BUFFER_FULL) return BMX_UART_ERROR_OVERRUN;
    return 0;
}

static void bmx_esp8266_uart_collect_events(void) {
    if (!bmx_esp8266_uart.event_queue || bmx_esp8266_uart.async_open) return;
    uart_event_t event;
    while (xQueueReceive(bmx_esp8266_uart.event_queue, &event, 0) == pdTRUE)
        bmx_esp8266_uart.errors |= bmx_esp8266_uart_error_for_event(event.type);
}

int32_t bmx_embedded_uart_controller_count(void) { return 1; }
int32_t bmx_embedded_uart_default_controller(void) { return 0; }
uint32_t bmx_embedded_uart_default_baudrate(void) { return 74880u; }

int32_t bmx_esp8266_uart_use_swapped_pins(int32_t enabled) {
    if (bmx_esp8266_uart.initialized || bmx_esp8266_uart.break_asserted) return 0;
    bmx_esp8266_uart_route_swapped(enabled);
    bmx_esp8266_uart.swapped = enabled != 0;
    bmx_esp8266_uart.configured = 1;
    return 1;
}

int32_t bmx_esp8266_uart_uses_swapped_pins(void) { return bmx_esp8266_uart.swapped; }

int32_t bmx_embedded_uart_configure_pins(int32_t controller, uint32_t tx_pin,
        uint32_t rx_pin) {
    if (controller != 0 || bmx_esp8266_uart.initialized) return 0;
    if (tx_pin == 1u && rx_pin == 3u) return bmx_esp8266_uart_use_swapped_pins(0);
    if (tx_pin == 15u && rx_pin == 13u) return bmx_esp8266_uart_use_swapped_pins(1);
    return 0;
}

int32_t bmx_embedded_uart_configure_flow_pins(int32_t controller,
        uint32_t cts_pin, uint32_t rts_pin) {
    if (controller != 0 || bmx_esp8266_uart.initialized || bmx_esp8266_uart.swapped ||
            cts_pin != 13u || rts_pin != 15u) return 0;
    bmx_esp8266_uart.flow_pins = 1;
    return 1;
}

uint32_t bmx_embedded_uart_init(int32_t controller, uint32_t baudrate) {
    if (controller != 0 || !bmx_esp8266_uart.configured ||
            bmx_esp8266_uart.initialized || !baudrate) return 0;
    uart_config_t config = {
        .baud_rate = (int)baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
    };
    if (uart_param_config(UART_NUM_0, &config) != ESP_OK) return 0;
    QueueHandle_t queue = NULL;
    /* Keep the SDK TX buffer disabled: the ESP8266 RTOS SDK 3.x completion
       semaphore can describe an earlier queued item when consecutive writes
       are pending, causing uart_wait_tx_done() to spin indefinitely. */
    if (uart_driver_install(UART_NUM_0, BMX_UART_RX_BUFFER, 0,
            BMX_UART_EVENT_CAPACITY, &queue, 0) != ESP_OK) return 0;
    bmx_esp8266_uart.event_queue = queue;
    bmx_esp8266_uart.errors = 0;
    bmx_esp8266_uart.translate_crlf = 0;
    bmx_esp8266_uart.break_asserted = 0;
    bmx_esp8266_uart.initialized = 1;
    return bmx_embedded_uart_set_baudrate(controller, baudrate);
}

int32_t bmx_embedded_uart_deinit(int32_t controller) {
    if (controller != 0 || !bmx_esp8266_uart.initialized || bmx_esp8266_uart.async_open)
        return 0;
    if (bmx_esp8266_uart.break_asserted && !bmx_embedded_uart_set_break(0, 0)) return 0;
    if (!bmx_esp8266_uart_wait_tx_done()) return 0;
    if (uart_driver_delete(UART_NUM_0) != ESP_OK) return 0;
    /* Leave the ROM console route usable after a portable UART session. */
    bmx_esp8266_uart_route_swapped(0);
    /* Driver teardown can rewrite peripheral state, so restore the console
       rate only after the SDK no longer owns UART0. */
    if (uart_set_baudrate(UART_NUM_0, 74880u) != ESP_OK) return 0;
    bmx_esp8266_uart.event_queue = NULL;
    bmx_esp8266_uart.initialized = 0;
    bmx_esp8266_uart.swapped = 0;
    bmx_esp8266_uart.configured = 1;
    bmx_esp8266_uart.errors = 0;
    return 1;
}

uint32_t bmx_embedded_uart_set_baudrate(int32_t controller, uint32_t baudrate) {
    if (controller != 0 || !bmx_esp8266_uart.initialized || !baudrate ||
            uart_set_baudrate(UART_NUM_0, baudrate) != ESP_OK) return 0;
    uint32_t actual = 0;
    return uart_get_baudrate(UART_NUM_0, &actual) == ESP_OK ? actual : 0;
}

int32_t bmx_embedded_uart_set_format(int32_t controller, uint32_t data_bits,
        uint32_t stop_bits, uint32_t parity) {
    if (controller != 0 || !bmx_esp8266_uart.initialized || data_bits < 5u ||
            data_bits > 8u || (stop_bits != 1u && stop_bits != 2u) || parity > 2u)
        return 0;
    uart_word_length_t bits = (uart_word_length_t)(data_bits - 5u);
    uart_stop_bits_t stops = stop_bits == 1u ? UART_STOP_BITS_1 : UART_STOP_BITS_2;
    uart_parity_t parity_mode = UART_PARITY_DISABLE;
    if (parity == 1u) parity_mode = UART_PARITY_EVEN;
    if (parity == 2u) parity_mode = UART_PARITY_ODD;
    return uart_set_word_length(UART_NUM_0, bits) == ESP_OK &&
        uart_set_stop_bits(UART_NUM_0, stops) == ESP_OK &&
        uart_set_parity(UART_NUM_0, parity_mode) == ESP_OK;
}

int32_t bmx_embedded_uart_set_flow_control(int32_t controller, int32_t cts,
        int32_t rts) {
    if (controller != 0 || !bmx_esp8266_uart.initialized ||
            ((cts || rts) && !bmx_esp8266_uart.flow_pins)) return 0;
    uart_hw_flowcontrol_t mode = UART_HW_FLOWCTRL_DISABLE;
    if (cts && rts) mode = UART_HW_FLOWCTRL_CTS_RTS;
    else if (cts) mode = UART_HW_FLOWCTRL_CTS;
    else if (rts) mode = UART_HW_FLOWCTRL_RTS;
    return uart_set_hw_flow_ctrl(UART_NUM_0, mode, UART_FIFO_LEN - 16) == ESP_OK;
}

int32_t bmx_embedded_uart_is_enabled(int32_t controller) {
    return controller == 0 && bmx_esp8266_uart.initialized;
}
int32_t bmx_embedded_uart_is_writable(int32_t controller) {
    return bmx_embedded_uart_is_enabled(controller) && !bmx_esp8266_uart.break_asserted;
}
int32_t bmx_embedded_uart_is_readable(int32_t controller) {
    if (!bmx_embedded_uart_is_enabled(controller)) return 0;
    size_t available = 0;
    return uart_get_buffered_data_len(UART_NUM_0, &available) == ESP_OK && available;
}
int32_t bmx_embedded_uart_is_readable_within_us(int32_t controller,
        uint32_t timeout_us) {
    if (!bmx_embedded_uart_is_enabled(controller)) return 0;
    int64_t deadline = esp_timer_get_time() + timeout_us;
    do {
        if (bmx_embedded_uart_is_readable(controller)) return 1;
        if (esp_timer_get_time() >= deadline) return 0;
        taskYIELD();
    } while (1);
}

static int32_t bmx_esp8266_uart_write_raw(const uint8_t *source, int32_t length) {
    if (bmx_esp8266_uart.break_asserted) return -1;
    return uart_write_bytes(UART_NUM_0, (const char *)source, (size_t)length) == length
        ? length : -1;
}

int32_t bmx_embedded_uart_write_blocking(int32_t controller,
        const void *source, int32_t length) {
    if (controller != 0 || !bmx_esp8266_uart_valid_buffer(source, length))
        return BMX_UART_INVALID_ARGUMENT;
    if (!length) return 0;
    const uint8_t *bytes = source;
    if (!bmx_esp8266_uart.translate_crlf) return bmx_esp8266_uart_write_raw(bytes, length);
    int32_t start = 0;
    for (int32_t index = 0; index < length; ++index) {
        if (bytes[index] != '\n') continue;
        if (index > start && bmx_esp8266_uart_write_raw(bytes + start, index - start) < 0)
            return -1;
        static const uint8_t crlf[2] = {'\r', '\n'};
        if (bmx_esp8266_uart_write_raw(crlf, 2) < 0) return -1;
        start = index + 1;
    }
    if (start < length && bmx_esp8266_uart_write_raw(bytes + start, length - start) < 0)
        return -1;
    return length;
}

int32_t bmx_embedded_uart_read_blocking(int32_t controller,
        void *destination, int32_t length) {
    if (controller != 0 || !bmx_esp8266_uart_valid_buffer(destination, length))
        return BMX_UART_INVALID_ARGUMENT;
    int32_t count = 0;
    while (count < length) {
        int result = uart_read_bytes(UART_NUM_0, (uint8_t *)destination + count,
            (uint32_t)(length - count), portMAX_DELAY);
        if (result < 0) return -1;
        count += result;
    }
    return count;
}

int32_t bmx_embedded_uart_read_timeout_us(int32_t controller,
        void *destination, int32_t length, uint32_t timeout_us) {
    if (controller != 0 || !bmx_esp8266_uart_valid_buffer(destination, length))
        return BMX_UART_INVALID_ARGUMENT;
    if (!length) return 0;
    return uart_read_bytes(UART_NUM_0, destination, (uint32_t)length,
        bmx_esp8266_uart_ticks(timeout_us));
}
int32_t bmx_embedded_uart_read_available(int32_t controller,
        void *destination, int32_t capacity) {
    if (controller != 0 || !bmx_esp8266_uart_valid_buffer(destination, capacity))
        return BMX_UART_INVALID_ARGUMENT;
    return capacity ? uart_read_bytes(UART_NUM_0, destination, (uint32_t)capacity, 0) : 0;
}
int32_t bmx_embedded_uart_put_byte(int32_t controller, uint32_t value) {
    if (controller != 0 || value > 0xffu || !bmx_esp8266_uart.initialized) return 0;
    uint8_t byte = (uint8_t)value;
    return bmx_embedded_uart_write_blocking(0, &byte, 1) == 1;
}
int32_t bmx_embedded_uart_tx_wait_blocking(int32_t controller) {
    return controller == 0 && bmx_esp8266_uart.initialized &&
        bmx_esp8266_uart_wait_tx_done();
}

int32_t bmx_embedded_uart_set_break(int32_t controller, int32_t enabled) {
    if (controller != 0 || !bmx_esp8266_uart.initialized) return 0;
    if ((enabled != 0) == bmx_esp8266_uart.break_asserted) return 1;
    uint32_t tx_pin = bmx_esp8266_uart.swapped ? 15u : 1u;
    if (enabled) {
        if (!bmx_esp8266_uart_wait_tx_done() ||
                gpio_set_direction((gpio_num_t)tx_pin, GPIO_MODE_OUTPUT) != ESP_OK ||
                gpio_set_level((gpio_num_t)tx_pin, 0) != ESP_OK) return 0;
    } else {
        bmx_esp8266_uart_route_swapped(bmx_esp8266_uart.swapped);
    }
    bmx_esp8266_uart.break_asserted = enabled != 0;
    return 1;
}
int32_t bmx_embedded_uart_set_translate_crlf(int32_t controller, int32_t enabled) {
    if (controller != 0 || !bmx_esp8266_uart.initialized) return 0;
    bmx_esp8266_uart.translate_crlf = enabled != 0;
    return 1;
}
uint32_t bmx_embedded_uart_get_errors(int32_t controller) {
    if (controller != 0 || !bmx_esp8266_uart.initialized) return 0;
    bmx_esp8266_uart_collect_events();
    return bmx_esp8266_uart.errors;
}
int32_t bmx_embedded_uart_clear_errors(int32_t controller) {
    if (controller != 0 || !bmx_esp8266_uart.initialized) return 0;
    bmx_esp8266_uart_collect_events();
    bmx_esp8266_uart.errors = 0;
    return 1;
}

static int32_t bmx_esp8266_uart_async_tx_complete(int32_t wait) {
    (void)wait;
    if (!bmx_esp8266_uart.async_tx_active) return 1;
    int64_t now = esp_timer_get_time();
    if (now < bmx_esp8266_uart.async_tx_deadline) return 0;
    bmx_esp8266_uart.async_tx_active = 0;
    return 1;
}

static void bmx_esp8266_uart_async_receive(void) {
    uint8_t input[64];
    int32_t result;
    do {
        result = uart_read_bytes(UART_NUM_0, input, sizeof(input), 0);
        if (result <= 0) break;
        portENTER_CRITICAL();
        int32_t was_empty = bmx_esp8266_uart.async_rx_put ==
            bmx_esp8266_uart.async_rx_get;
        for (int32_t index = 0; index < result; ++index) {
            uint32_t put = bmx_esp8266_uart.async_rx_put;
            if (put - bmx_esp8266_uart.async_rx_get >=
                    bmx_esp8266_uart.async_rx_capacity) {
                if (bmx_esp8266_uart.async_rx_dropped != UINT32_MAX)
                    ++bmx_esp8266_uart.async_rx_dropped;
            } else {
                bmx_esp8266_uart.async_rx_buffer[put &
                    (bmx_esp8266_uart.async_rx_capacity - 1u)] = input[index];
                bmx_esp8266_uart.async_rx_put = put + 1u;
            }
        }
        uint32_t available = bmx_esp8266_uart.async_rx_put -
            bmx_esp8266_uart.async_rx_get;
        uint32_t token = bmx_esp8266_uart.async_rx_token;
        portEXIT_CRITICAL();
        if (was_empty && available)
            (void)bmx_embedded_event_post(token, available, 0);
    } while (result == (int32_t)sizeof(input));
}

static void bmx_esp8266_uart_async_transmit(void) {
    if (!bmx_esp8266_uart_async_tx_complete(0)) return;
    uint8_t output[UART_FIFO_LEN];
    int32_t count = 0;
    portENTER_CRITICAL();
    while (count < (int32_t)sizeof(output) &&
            bmx_esp8266_uart.async_tx_get != bmx_esp8266_uart.async_tx_put) {
        output[count++] = bmx_esp8266_uart.async_tx_buffer[
            bmx_esp8266_uart.async_tx_get &
            (bmx_esp8266_uart.async_tx_capacity - 1u)];
        ++bmx_esp8266_uart.async_tx_get;
    }
    portEXIT_CRITICAL();
    if (!count) return;
    /* GPIO-swapped UART0 can retain a stale FIFO count after the shifter has
       drained. The baud-derived completion deadline proves this is safe. */
    portENTER_CRITICAL();
    SET_PERI_REG_MASK(UART_CONF0(UART_NUM_0), UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(UART_NUM_0), UART_TXFIFO_RST);
    portEXIT_CRITICAL();
    int32_t written = uart_tx_chars(UART_NUM_0, (const char *)output,
        (uint32_t)count);
    if (written < count) {
        portENTER_CRITICAL();
        bmx_esp8266_uart.async_tx_get -= (uint32_t)(count -
            (written > 0 ? written : 0));
        portEXIT_CRITICAL();
    }
    if (written <= 0) return;
    uint32_t baudrate = 0;
    if (uart_get_baudrate(UART_NUM_0, &baudrate) != ESP_OK || !baudrate) {
        bmx_esp8266_uart.errors |= BMX_UART_ERROR_OVERRUN;
        (void)bmx_embedded_event_post(bmx_esp8266_uart.async_error_token,
            BMX_UART_ERROR_OVERRUN, 0);
        return;
    }
    int64_t duration = ((int64_t)written * 12 * 1000000 + baudrate - 1) /
        baudrate + 2000;
    bmx_esp8266_uart.async_tx_deadline = esp_timer_get_time() + duration;
    bmx_esp8266_uart.async_tx_active = 1;
}

static void bmx_esp8266_uart_async_service(TimerHandle_t timer) {
    (void)timer;
    if (!bmx_esp8266_uart.async_open) return;
    uart_event_t event;
    while (xQueueReceive(bmx_esp8266_uart.event_queue, &event, 0) == pdTRUE) {
        uint32_t error = bmx_esp8266_uart_error_for_event(event.type);
        if (!error) continue;
        bmx_esp8266_uart.errors |= error;
        if (bmx_esp8266_uart.async_rx_dropped != UINT32_MAX)
            ++bmx_esp8266_uart.async_rx_dropped;
        (void)bmx_embedded_event_post(bmx_esp8266_uart.async_error_token, error, 0);
    }
    bmx_esp8266_uart_async_receive();
    int32_t was_active = bmx_esp8266_uart.async_tx_active;
    bmx_esp8266_uart_async_transmit();
    portENTER_CRITICAL();
    int32_t tx_empty = bmx_esp8266_uart.async_tx_get ==
        bmx_esp8266_uart.async_tx_put;
    int32_t tx_active = bmx_esp8266_uart.async_tx_active;
    uint32_t tx_token = bmx_esp8266_uart.async_tx_token;
    portEXIT_CRITICAL();
    if (was_active && !tx_active && tx_empty)
        (void)bmx_embedded_event_post(tx_token, 0, 0);
}

int32_t bmx_embedded_uart_async_open(int32_t controller, uint32_t rx_capacity,
        uint32_t tx_capacity, uint32_t rx_token, uint32_t tx_token,
        uint32_t error_token) {
    if (controller != 0 || !bmx_esp8266_uart.initialized || bmx_esp8266_uart.async_open ||
            rx_capacity < 2u || tx_capacity < 2u || (rx_capacity & (rx_capacity - 1u)) ||
            (tx_capacity & (tx_capacity - 1u)) || !rx_token || !tx_token || !error_token)
        return 0;
    uint8_t *rx_buffer = pvPortMalloc(rx_capacity);
    if (!rx_buffer) return 0;
    uint8_t *tx_buffer = pvPortMalloc(tx_capacity);
    if (!tx_buffer) {
        vPortFree(rx_buffer);
        return 0;
    }
    bmx_esp8266_uart.async_rx_buffer = rx_buffer;
    bmx_esp8266_uart.async_tx_buffer = tx_buffer;
    bmx_esp8266_uart.async_rx_capacity = rx_capacity;
    bmx_esp8266_uart.async_tx_capacity = tx_capacity;
    bmx_esp8266_uart.async_rx_put = bmx_esp8266_uart.async_rx_get = 0;
    bmx_esp8266_uart.async_tx_put = bmx_esp8266_uart.async_tx_get = 0;
    bmx_esp8266_uart.async_rx_dropped = 0;
    bmx_esp8266_uart.async_rx_token = rx_token;
    bmx_esp8266_uart.async_tx_token = tx_token;
    bmx_esp8266_uart.async_error_token = error_token;
    bmx_esp8266_uart.async_tx_active = 0;
    bmx_esp8266_uart.async_timer = xTimerCreate("bmx-uart", 1, pdTRUE, NULL,
        bmx_esp8266_uart_async_service);
    if (!bmx_esp8266_uart.async_timer) {
        vPortFree(rx_buffer);
        vPortFree(tx_buffer);
        bmx_esp8266_uart.async_rx_buffer = bmx_esp8266_uart.async_tx_buffer = NULL;
        return 0;
    }
    bmx_esp8266_uart.async_open = 1;
    if (xTimerStart(bmx_esp8266_uart.async_timer, portMAX_DELAY) != pdPASS) {
        bmx_esp8266_uart.async_open = 0;
        (void)xTimerDelete(bmx_esp8266_uart.async_timer, portMAX_DELAY);
        bmx_esp8266_uart.async_timer = NULL;
        vPortFree(rx_buffer);
        vPortFree(tx_buffer);
        bmx_esp8266_uart.async_rx_buffer = bmx_esp8266_uart.async_tx_buffer = NULL;
        return 0;
    }
    return 1;
}

int32_t bmx_embedded_uart_async_close(int32_t controller) {
    if (controller != 0) return 0;
    if (!bmx_esp8266_uart.async_open) return 1;
    bmx_esp8266_uart.async_open = 0;
    if (bmx_esp8266_uart.async_timer) {
        (void)xTimerStop(bmx_esp8266_uart.async_timer, portMAX_DELAY);
        (void)xTimerDelete(bmx_esp8266_uart.async_timer, portMAX_DELAY);
        bmx_esp8266_uart.async_timer = NULL;
    }
    uint8_t *rx_buffer = bmx_esp8266_uart.async_rx_buffer;
    uint8_t *tx_buffer = bmx_esp8266_uart.async_tx_buffer;
    bmx_esp8266_uart.async_rx_buffer = bmx_esp8266_uart.async_tx_buffer = NULL;
    bmx_esp8266_uart.async_rx_capacity = bmx_esp8266_uart.async_tx_capacity = 0;
    bmx_esp8266_uart.async_rx_put = bmx_esp8266_uart.async_rx_get = 0;
    bmx_esp8266_uart.async_tx_put = bmx_esp8266_uart.async_tx_get = 0;
    vPortFree(rx_buffer);
    vPortFree(tx_buffer);
    return 1;
}
int32_t bmx_embedded_uart_async_is_open(int32_t controller) {
    return controller == 0 && bmx_esp8266_uart.async_open;
}
int32_t bmx_embedded_uart_async_read(int32_t controller, void *destination,
        int32_t capacity) {
    if (controller != 0 || !bmx_esp8266_uart.async_open || capacity < 0 ||
            (capacity && !destination)) return BMX_UART_INVALID_ARGUMENT;
    uint8_t *bytes = destination;
    portENTER_CRITICAL();
    int32_t count = 0;
    while (count < capacity && bmx_esp8266_uart.async_rx_get !=
            bmx_esp8266_uart.async_rx_put) {
        bytes[count++] = bmx_esp8266_uart.async_rx_buffer[
            bmx_esp8266_uart.async_rx_get &
            (bmx_esp8266_uart.async_rx_capacity - 1u)];
        ++bmx_esp8266_uart.async_rx_get;
    }
    portEXIT_CRITICAL();
    return count;
}
int32_t bmx_embedded_uart_async_write(int32_t controller, const void *source,
        int32_t length) {
    if (controller != 0 || !bmx_esp8266_uart.async_open || length < 0 ||
            (length && !source)) return BMX_UART_INVALID_ARGUMENT;
    if (!length) return 0;
    portENTER_CRITICAL();
    const uint8_t *bytes = source;
    int32_t count = 0;
    while (count < length && bmx_esp8266_uart.async_tx_put -
            bmx_esp8266_uart.async_tx_get < bmx_esp8266_uart.async_tx_capacity) {
        bmx_esp8266_uart.async_tx_buffer[bmx_esp8266_uart.async_tx_put &
            (bmx_esp8266_uart.async_tx_capacity - 1u)] = bytes[count++];
        ++bmx_esp8266_uart.async_tx_put;
    }
    portEXIT_CRITICAL();
    return count;
}
uint32_t bmx_embedded_uart_async_read_available(int32_t controller) {
    if (controller != 0 || !bmx_esp8266_uart.async_open) return 0;
    portENTER_CRITICAL();
    uint32_t available = bmx_esp8266_uart.async_rx_put -
        bmx_esp8266_uart.async_rx_get;
    portEXIT_CRITICAL();
    return available;
}
uint32_t bmx_embedded_uart_async_write_available(int32_t controller) {
    if (controller != 0 || !bmx_esp8266_uart.async_open) return 0;
    portENTER_CRITICAL();
    uint32_t available = bmx_esp8266_uart.async_tx_capacity -
        (bmx_esp8266_uart.async_tx_put - bmx_esp8266_uart.async_tx_get);
    portEXIT_CRITICAL();
    return available;
}
uint32_t bmx_embedded_uart_async_rx_dropped(int32_t controller) {
    return controller == 0 ? bmx_esp8266_uart.async_rx_dropped : 0;
}
int32_t bmx_embedded_uart_async_tx_idle(int32_t controller) {
    if (controller != 0 || !bmx_esp8266_uart.async_open) return 1;
    portENTER_CRITICAL();
    int32_t idle = bmx_esp8266_uart.async_tx_put ==
        bmx_esp8266_uart.async_tx_get && !bmx_esp8266_uart.async_tx_active;
    portEXIT_CRITICAL();
    return idle;
}
