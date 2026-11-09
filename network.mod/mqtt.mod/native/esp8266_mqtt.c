/* Copyright (c) 2026 Bruce A Henderson and contributors. SPDX-License-Identifier: Zlib */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"

#define BMX_MQTT_EVENT_COUNT 8u
#define BMX_MQTT_TOPIC_MAX 127u
#define BMX_MQTT_PAYLOAD_MAX 1024u

typedef struct BMXESP8266MQTTEvent {
    int32_t kind;
    int32_t code;
    int32_t qos;
    int32_t retained;
    uint32_t topic_length;
    uint32_t payload_length;
    char topic[BMX_MQTT_TOPIC_MAX + 1];
    uint8_t payload[BMX_MQTT_PAYLOAD_MAX];
} BMXESP8266MQTTEvent;

static BMXESP8266MQTTEvent bmx_mqtt_events[BMX_MQTT_EVENT_COUNT];
static BMXESP8266MQTTEvent bmx_mqtt_incoming;
static volatile uint32_t bmx_mqtt_put;
static volatile uint32_t bmx_mqtt_get;
static volatile uint32_t bmx_mqtt_dropped;
static esp_mqtt_client_handle_t bmx_mqtt_client;
static volatile int32_t bmx_mqtt_state;
static bool bmx_mqtt_receiving;
static bool bmx_mqtt_overflow;
static char bmx_mqtt_uri[320];
static char bmx_mqtt_client_id[128];
static char bmx_mqtt_user[128];
static char bmx_mqtt_password[128];

static void bmx_mqtt_queue(const BMXESP8266MQTTEvent *event) {
    taskENTER_CRITICAL();
    if (bmx_mqtt_put - bmx_mqtt_get == BMX_MQTT_EVENT_COUNT) {
        if (bmx_mqtt_dropped != UINT32_MAX) ++bmx_mqtt_dropped;
        ++bmx_mqtt_get;
    }
    bmx_mqtt_events[bmx_mqtt_put % BMX_MQTT_EVENT_COUNT] = *event;
    ++bmx_mqtt_put;
    taskEXIT_CRITICAL();
}

static void bmx_mqtt_handle(void *argument, esp_event_base_t base,
        int32_t event_id, void *data) {
    (void)argument;
    (void)base;
    esp_mqtt_event_handle_t event = data;
    BMXESP8266MQTTEvent record = {0};
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            bmx_mqtt_state = 2;
            record.kind = 1;
            bmx_mqtt_queue(&record);
            break;
        case MQTT_EVENT_DISCONNECTED:
            bmx_mqtt_state = 0;
            record.kind = 2;
            bmx_mqtt_queue(&record);
            break;
        case MQTT_EVENT_ERROR:
            record.kind = 4;
            record.code = event && event->error_handle
                ? event->error_handle->error_type : -1;
            bmx_mqtt_queue(&record);
            break;
        case MQTT_EVENT_DATA:
            if (!event) break;
            if (event->current_data_offset == 0) {
                memset(&bmx_mqtt_incoming, 0, sizeof(bmx_mqtt_incoming));
                bmx_mqtt_incoming.kind = 3;
                bmx_mqtt_incoming.topic_length = event->topic_len;
                bmx_mqtt_overflow = event->topic_len < 0 ||
                    event->topic_len > BMX_MQTT_TOPIC_MAX ||
                    event->total_data_len < 0 ||
                    event->total_data_len > BMX_MQTT_PAYLOAD_MAX;
                if (!bmx_mqtt_overflow && event->topic_len)
                    memcpy(bmx_mqtt_incoming.topic, event->topic,
                        (size_t)event->topic_len);
                bmx_mqtt_receiving = true;
            }
            if (!bmx_mqtt_receiving) break;
            if (event->data_len < 0 ||
                    bmx_mqtt_incoming.payload_length +
                    (uint32_t)event->data_len > BMX_MQTT_PAYLOAD_MAX) {
                bmx_mqtt_overflow = true;
            } else if (!bmx_mqtt_overflow && event->data_len) {
                memcpy(bmx_mqtt_incoming.payload +
                    bmx_mqtt_incoming.payload_length, event->data,
                    (size_t)event->data_len);
                bmx_mqtt_incoming.payload_length += (uint32_t)event->data_len;
            }
            if (event->current_data_offset + event->data_len >=
                    event->total_data_len) {
                if (!bmx_mqtt_overflow) {
                    bmx_mqtt_queue(&bmx_mqtt_incoming);
                } else {
                    record.kind = 4;
                    record.code = -4;
                    bmx_mqtt_queue(&record);
                }
                bmx_mqtt_receiving = false;
            }
            break;
    }
}

static int bmx_mqtt_copy(char *destination, size_t size,
        const uint8_t *source, bool required) {
    if (!source) {
        destination[0] = 0;
        return required ? -1 : 0;
    }
    size_t length = strnlen((const char *)source, size);
    if (length == size || (required && !length)) return -1;
    memcpy(destination, source, length + 1);
    return 0;
}

void bmx_embedded_mqtt_stop(void) {
    if (bmx_mqtt_client) {
        (void)esp_mqtt_client_stop(bmx_mqtt_client);
        (void)esp_mqtt_client_destroy(bmx_mqtt_client);
        bmx_mqtt_client = NULL;
    }
    taskENTER_CRITICAL();
    bmx_mqtt_state = 0;
    bmx_mqtt_put = bmx_mqtt_get;
    bmx_mqtt_receiving = false;
    taskEXIT_CRITICAL();
}

int32_t bmx_embedded_mqtt_start(const uint8_t *host, int32_t port,
        const uint8_t *client_id, const uint8_t *user,
        const uint8_t *password, int32_t keep_alive) {
    if (port < 1 || port > 65535 || keep_alive < 0 || keep_alive > 65535 ||
            bmx_mqtt_copy(bmx_mqtt_client_id, sizeof(bmx_mqtt_client_id),
                client_id, true) ||
            bmx_mqtt_copy(bmx_mqtt_user, sizeof(bmx_mqtt_user), user, false) ||
            bmx_mqtt_copy(bmx_mqtt_password, sizeof(bmx_mqtt_password),
                password, false))
        return -1;
    char hostname[256];
    if (bmx_mqtt_copy(hostname, sizeof(hostname), host, true)) return -1;
    int length = snprintf(bmx_mqtt_uri, sizeof(bmx_mqtt_uri),
        "mqtt://%s:%d", hostname, (int)port);
    if (length < 0 || (size_t)length >= sizeof(bmx_mqtt_uri)) return -1;

    bmx_embedded_mqtt_stop();
    esp_mqtt_client_config_t config = {
        .uri = bmx_mqtt_uri,
        .client_id = bmx_mqtt_client_id,
        .username = user ? bmx_mqtt_user : NULL,
        .password = password ? bmx_mqtt_password : NULL,
        .keepalive = keep_alive,
        .disable_auto_reconnect = true,
    };
    bmx_mqtt_client = esp_mqtt_client_init(&config);
    if (!bmx_mqtt_client) return -2;
    esp_err_t result = esp_mqtt_client_register_event(bmx_mqtt_client,
        MQTT_EVENT_ANY, bmx_mqtt_handle, NULL);
    bmx_mqtt_state = 1;
    if (result == ESP_OK)
        result = esp_mqtt_client_start(bmx_mqtt_client);
    if (result != ESP_OK) {
        bmx_embedded_mqtt_stop();
        return result;
    }
    return 0;
}

int32_t bmx_embedded_mqtt_state(void) {
    return bmx_mqtt_state;
}

int32_t bmx_embedded_mqtt_publish(const uint8_t *topic,
        const uint8_t *payload, int32_t length, int32_t qos,
        int32_t retained) {
    if (!bmx_mqtt_client || bmx_mqtt_state != 2 || !topic || length < 0 ||
            (length && !payload) || qos < 0 || qos > 2)
        return -1;
    int result = esp_mqtt_client_publish(bmx_mqtt_client,
        (const char *)topic, (const char *)payload, length, qos, !!retained);
    return result < 0 ? result : 0;
}

int32_t bmx_embedded_mqtt_subscribe(const uint8_t *topic, int32_t qos) {
    if (!bmx_mqtt_client || bmx_mqtt_state != 2 || !topic ||
            qos < 0 || qos > 2)
        return -1;
    int result = esp_mqtt_client_subscribe(bmx_mqtt_client,
        (const char *)topic, qos);
    return result < 0 ? result : 0;
}

int32_t bmx_embedded_mqtt_take_event(int32_t *kind, int32_t *code,
        uint8_t *topic, int32_t *topic_length, uint8_t *payload,
        int32_t *payload_length, int32_t *qos, int32_t *retained) {
    if (!kind || !code || !topic || !topic_length || !payload ||
            !payload_length || !qos || !retained)
        return 0;
    taskENTER_CRITICAL();
    if (bmx_mqtt_get == bmx_mqtt_put) {
        taskEXIT_CRITICAL();
        return 0;
    }
    BMXESP8266MQTTEvent *event =
        &bmx_mqtt_events[bmx_mqtt_get % BMX_MQTT_EVENT_COUNT];
    *kind = event->kind;
    *code = event->code;
    *topic_length = (int32_t)event->topic_length;
    *payload_length = (int32_t)event->payload_length;
    *qos = event->qos;
    *retained = event->retained;
    memcpy(topic, event->topic, event->topic_length);
    memcpy(payload, event->payload, event->payload_length);
    ++bmx_mqtt_get;
    taskEXIT_CRITICAL();
    return 1;
}

uint32_t bmx_embedded_mqtt_dropped_events(void) {
    uint32_t result;
    taskENTER_CRITICAL();
    result = bmx_mqtt_dropped;
    taskEXIT_CRITICAL();
    return result;
}
