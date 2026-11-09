#include <stdint.h>

#include "blitzmax/embedded_events.h"
#include "blitzmax/embedded_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define BMX_ESP8266_EVENT_CAPACITY 32u
#define BMX_ESP8266_EVENT_MASK (BMX_ESP8266_EVENT_CAPACITY - 1u)

typedef struct BMXESP8266DeferredEvent {
    uint32_t token;
    uint32_t event_data;
    uint32_t event_mods;
    uint32_t event_x;
    uint32_t event_y;
} BMXESP8266DeferredEvent;

static BMXESP8266DeferredEvent bmx_esp8266_deferred_events[BMX_ESP8266_EVENT_CAPACITY];
static volatile uint32_t bmx_esp8266_deferred_event_put;
static volatile uint32_t bmx_esp8266_deferred_event_get;
static volatile uint32_t bmx_esp8266_deferred_event_dropped;
static SemaphoreHandle_t bmx_esp8266_deferred_event_signal;

static int32_t bmx_esp8266_event_post_locked(uint32_t token,
        uint32_t event_data, uint32_t event_mods, uint32_t event_x,
        uint32_t event_y) {
    if (bmx_esp8266_deferred_event_put - bmx_esp8266_deferred_event_get >=
            BMX_ESP8266_EVENT_CAPACITY) {
        if (bmx_esp8266_deferred_event_dropped != UINT32_MAX)
            ++bmx_esp8266_deferred_event_dropped;
        return 0;
    }
    BMXESP8266DeferredEvent *event = &bmx_esp8266_deferred_events[
        bmx_esp8266_deferred_event_put & BMX_ESP8266_EVENT_MASK];
    event->token = token;
    event->event_data = event_data;
    event->event_mods = event_mods;
    event->event_x = event_x;
    event->event_y = event_y;
    ++bmx_esp8266_deferred_event_put;
    return 1;
}

int32_t bmx_embedded_event_post(uint32_t token, uint32_t event_data,
        uint32_t detail) {
    return bmx_embedded_event_post_ex(token, event_data, 0, detail, 0);
}

int32_t bmx_embedded_event_post_ex(uint32_t token, uint32_t event_data,
        uint32_t event_mods, uint32_t event_x, uint32_t event_y) {
    if (!token) return 0;
    taskENTER_CRITICAL();
    int32_t posted = bmx_esp8266_event_post_locked(token, event_data,
        event_mods, event_x, event_y);
    SemaphoreHandle_t signal = bmx_esp8266_deferred_event_signal;
    taskEXIT_CRITICAL();
    if (posted && signal) (void)xSemaphoreGive(signal);
    return posted;
}

int32_t bmx_embedded_event_post_from_isr(uint32_t token,
        uint32_t event_data, uint32_t detail) {
    return bmx_embedded_event_post_from_isr_ex(token, event_data, 0, detail, 0);
}

int32_t bmx_embedded_event_post_from_isr_ex(uint32_t token,
        uint32_t event_data, uint32_t event_mods, uint32_t event_x,
        uint32_t event_y) {
    if (!token) return 0;
    UBaseType_t state = taskENTER_CRITICAL_FROM_ISR();
    int32_t posted = bmx_esp8266_event_post_locked(token, event_data,
        event_mods, event_x, event_y);
    SemaphoreHandle_t signal = bmx_esp8266_deferred_event_signal;
    taskEXIT_CRITICAL_FROM_ISR(state);
    if (posted && signal) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        (void)xSemaphoreGiveFromISR(signal, &higher_priority_task_woken);
        if (higher_priority_task_woken) portYIELD_FROM_ISR();
    }
    return posted;
}

int32_t bmx_embedded_event_take(uint32_t *token, uint32_t *event_data,
        uint32_t *event_mods, uint32_t *event_x, uint32_t *event_y) {
    if (!token || !event_data || !event_mods || !event_x || !event_y) return 0;
    taskENTER_CRITICAL();
    if (bmx_esp8266_deferred_event_get == bmx_esp8266_deferred_event_put) {
        taskEXIT_CRITICAL();
        return 0;
    }
    BMXESP8266DeferredEvent event = bmx_esp8266_deferred_events[
        bmx_esp8266_deferred_event_get & BMX_ESP8266_EVENT_MASK];
    ++bmx_esp8266_deferred_event_get;
    taskEXIT_CRITICAL();
    *token = event.token;
    *event_data = event.event_data;
    *event_mods = event.event_mods;
    *event_x = event.event_x;
    *event_y = event.event_y;
    return 1;
}

uint32_t bmx_embedded_event_pending(void) {
    taskENTER_CRITICAL();
    uint32_t pending = bmx_esp8266_deferred_event_put -
        bmx_esp8266_deferred_event_get;
    taskEXIT_CRITICAL();
    return pending;
}

uint32_t bmx_embedded_event_dropped(void) {
    taskENTER_CRITICAL();
    uint32_t dropped = bmx_esp8266_deferred_event_dropped;
    taskEXIT_CRITICAL();
    return dropped;
}

static SemaphoreHandle_t bmx_esp8266_event_signal(void) {
    SemaphoreHandle_t signal = bmx_esp8266_deferred_event_signal;
    if (signal) return signal;

    /* FreeRTOS object creation allocates memory and must not run with
       interrupts disabled.  Create a candidate first, then publish it under
       the short critical section.  A racing caller may win publication; in
       that case discard our unused candidate after leaving the section. */
    SemaphoreHandle_t candidate = xSemaphoreCreateBinary();
    if (!candidate) return NULL;
    taskENTER_CRITICAL();
    if (!bmx_esp8266_deferred_event_signal) {
        bmx_esp8266_deferred_event_signal = candidate;
        candidate = NULL;
    }
    signal = bmx_esp8266_deferred_event_signal;
    taskEXIT_CRITICAL();
    if (candidate) vSemaphoreDelete(candidate);
    return signal;
}

void bmx_embedded_system_wait(void) {
    SemaphoreHandle_t signal = bmx_esp8266_event_signal();
    taskENTER_CRITICAL();
    if (bmx_esp8266_deferred_event_get != bmx_esp8266_deferred_event_put) {
        taskEXIT_CRITICAL();
        return;
    }
    taskEXIT_CRITICAL();
    if (signal) (void)xSemaphoreTake(signal, portMAX_DELAY);
}

void bmx_embedded_system_wake(void) {
    SemaphoreHandle_t signal = bmx_esp8266_event_signal();
    if (signal) (void)xSemaphoreGive(signal);
}
