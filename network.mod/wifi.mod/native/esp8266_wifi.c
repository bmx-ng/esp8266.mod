#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"

#define BMX_WIFI_EVENT_CAPACITY 32u
#define BMX_WIFI_EVENT_MASK (BMX_WIFI_EVENT_CAPACITY - 1u)
#define BMX_WIFI_SCAN_CAPACITY 16u

#define BMX_WIFI_SCAN_RESULT 1
#define BMX_WIFI_SCAN_COMPLETE 2
#define BMX_WIFI_LINK_STATE 3

#define BMX_WIFI_LINK_DOWN 0
#define BMX_WIFI_LINK_JOINED 1
#define BMX_WIFI_LINK_NO_IP 2
#define BMX_WIFI_LINK_UP 3
#define BMX_WIFI_LINK_FAILED -1
#define BMX_WIFI_LINK_NO_NETWORK -2
#define BMX_WIFI_LINK_BAD_AUTHENTICATION -3

typedef struct BMXESP8266WiFiEvent {
    int32_t kind;
    uint8_t ssid_length;
    uint8_t ssid[32];
    uint8_t bssid[6];
    uint16_t channel;
    int16_t rssi;
    uint8_t security;
    int32_t link_status;
    uint32_t address;
    uint32_t netmask;
    uint32_t gateway;
} BMXESP8266WiFiEvent;

static BMXESP8266WiFiEvent bmx_wifi_events[BMX_WIFI_EVENT_CAPACITY];
static wifi_ap_record_t bmx_wifi_scan_records[BMX_WIFI_SCAN_CAPACITY];
static volatile uint32_t bmx_wifi_event_put;
static volatile uint32_t bmx_wifi_event_get;
static volatile uint32_t bmx_wifi_dropped;
static volatile bool bmx_wifi_initialized;
static volatile bool bmx_wifi_scan_active;
static volatile bool bmx_wifi_explicit_disconnect;
static volatile int32_t bmx_wifi_link_status;

static bool bmx_wifi_queue(const BMXESP8266WiFiEvent *event) {
    bool queued = false;
    taskENTER_CRITICAL();
    if (bmx_wifi_event_put - bmx_wifi_event_get < BMX_WIFI_EVENT_CAPACITY) {
        bmx_wifi_events[bmx_wifi_event_put & BMX_WIFI_EVENT_MASK] = *event;
        ++bmx_wifi_event_put;
        queued = true;
    } else if (bmx_wifi_dropped != UINT32_MAX) {
        ++bmx_wifi_dropped;
    }
    taskEXIT_CRITICAL();
    if (queued) bmx_embedded_system_wake();
    return queued;
}

static uint8_t bmx_wifi_security(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN: return 0;
        case WIFI_AUTH_WEP: return 1;
        case WIFI_AUTH_WPA_PSK: return 2;
        case WIFI_AUTH_WPA2_PSK: return 4;
        case WIFI_AUTH_WPA_WPA2_PSK: return 2 | 4;
        case WIFI_AUTH_WPA3_PSK: return 8;
        case WIFI_AUTH_WPA2_WPA3_PSK: return 4 | 8;
        default: return 0xff;
    }
}

static void bmx_wifi_queue_link(int32_t status,
        const tcpip_adapter_ip_info_t *info) {
    BMXESP8266WiFiEvent event = {0};
    event.kind = BMX_WIFI_LINK_STATE;
    event.link_status = status;
    if (info) {
        event.address = info->ip.addr;
        event.netmask = info->netmask.addr;
        event.gateway = info->gw.addr;
    }
    bmx_wifi_link_status = status;
    (void)bmx_wifi_queue(&event);
}

static int32_t bmx_wifi_disconnect_status(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND:
            return BMX_WIFI_LINK_NO_NETWORK;
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_ASSOC_NOT_AUTHED:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
        case WIFI_REASON_IE_IN_4WAY_DIFFERS:
        case WIFI_REASON_802_1X_AUTH_FAILED:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return BMX_WIFI_LINK_BAD_AUTHENTICATION;
        default:
            return BMX_WIFI_LINK_FAILED;
    }
}

static void bmx_wifi_scan_done(void) {
    uint16_t available = BMX_WIFI_SCAN_CAPACITY;
    if (esp_wifi_scan_get_ap_records(&available, bmx_wifi_scan_records) == ESP_OK) {
        for (uint16_t index = 0; index < available; ++index) {
            const wifi_ap_record_t *record = &bmx_wifi_scan_records[index];
            BMXESP8266WiFiEvent event = {0};
            event.kind = BMX_WIFI_SCAN_RESULT;
            event.ssid_length = (uint8_t)strnlen((const char *)record->ssid,
                sizeof(event.ssid));
            memcpy(event.ssid, record->ssid, event.ssid_length);
            memcpy(event.bssid, record->bssid, sizeof(event.bssid));
            event.channel = record->primary;
            event.rssi = record->rssi;
            event.security = bmx_wifi_security(record->authmode);
            (void)bmx_wifi_queue(&event);
        }
    }
    BMXESP8266WiFiEvent complete = {0};
    complete.kind = BMX_WIFI_SCAN_COMPLETE;
    bmx_wifi_scan_active = false;
    (void)bmx_wifi_queue(&complete);
}

static void bmx_wifi_handle(void *argument, esp_event_base_t base,
        int32_t id, void *data) {
    (void)argument;
    if (base == WIFI_EVENT) {
        switch (id) {
            case WIFI_EVENT_SCAN_DONE:
                bmx_wifi_scan_done();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                bmx_wifi_queue_link(BMX_WIFI_LINK_JOINED, NULL);
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *event = data;
                int32_t status = bmx_wifi_explicit_disconnect
                    ? BMX_WIFI_LINK_DOWN
                    : bmx_wifi_disconnect_status(event ? event->reason : 0);
                bmx_wifi_explicit_disconnect = false;
                bmx_wifi_queue_link(status, NULL);
                break;
            }
            case WIFI_EVENT_STA_STOP:
                bmx_wifi_queue_link(BMX_WIFI_LINK_DOWN, NULL);
                break;
        }
    } else if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = data;
            bmx_wifi_queue_link(BMX_WIFI_LINK_UP,
                event ? &event->ip_info : NULL);
        } else if (id == IP_EVENT_STA_LOST_IP) {
            bmx_wifi_queue_link(BMX_WIFI_LINK_NO_IP, NULL);
        }
    }
}

static wifi_country_t bmx_wifi_country(uint32_t packed) {
    wifi_country_t country = {0};
    country.cc[0] = (char)(packed & 0xffu);
    country.cc[1] = (char)((packed >> 8) & 0xffu);
    country.cc[2] = 0;
    country.schan = 1;
    country.nchan = country.cc[0] == 'U' && country.cc[1] == 'S' ? 11 : 13;
    country.max_tx_power = 20;
    country.policy = WIFI_COUNTRY_POLICY_MANUAL;
    if (country.cc[0] == 'X' && country.cc[1] == 'X') {
        country.cc[0] = '0';
        country.cc[1] = '1';
        country.nchan = 11;
    }
    return country;
}

int32_t bmx_embedded_wifi_initialize(uint32_t country_code) {
    if (bmx_wifi_initialized) return ESP_OK;
    bool wifi_initialized = false;
    bool wifi_handler_registered = false;
    bool ip_handler_registered = false;
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES) {
        result = nvs_flash_erase();
        if (result == ESP_OK) result = nvs_flash_init();
    }
    if (result != ESP_OK) return result;
    tcpip_adapter_init();
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&config);
    if (result != ESP_OK) goto fail;
    wifi_initialized = true;
    result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        bmx_wifi_handle, NULL);
    if (result != ESP_OK) goto fail;
    wifi_handler_registered = true;
    result = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
        bmx_wifi_handle, NULL);
    if (result != ESP_OK) goto fail;
    ip_handler_registered = true;
    wifi_country_t country = bmx_wifi_country(country_code);
    result = esp_wifi_set_country(&country);
    if (result != ESP_OK) goto fail;
    result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result != ESP_OK) goto fail;
    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result != ESP_OK) goto fail;
    result = esp_wifi_start();
    if (result != ESP_OK) goto fail;
    bmx_wifi_event_put = 0;
    bmx_wifi_event_get = 0;
    bmx_wifi_dropped = 0;
    bmx_wifi_scan_active = false;
    bmx_wifi_explicit_disconnect = false;
    bmx_wifi_link_status = BMX_WIFI_LINK_DOWN;
    bmx_wifi_initialized = true;
    return ESP_OK;

fail:
    if (ip_handler_registered)
        (void)esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID,
            bmx_wifi_handle);
    if (wifi_handler_registered)
        (void)esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
            bmx_wifi_handle);
    if (wifi_initialized) (void)esp_wifi_deinit();
    return result;
}

int32_t bmx_embedded_wifi_deinitialize(void) {
    if (!bmx_wifi_initialized) return ESP_OK;
    if (bmx_wifi_scan_active) return ESP_ERR_INVALID_STATE;
    esp_err_t result = esp_wifi_stop();
    if (result != ESP_OK) return result;
    (void)esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID,
        bmx_wifi_handle);
    (void)esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
        bmx_wifi_handle);
    result = esp_wifi_deinit();
    if (result != ESP_OK) return result;
    bmx_wifi_initialized = false;
    bmx_wifi_event_put = bmx_wifi_event_get = 0;
    bmx_wifi_link_status = BMX_WIFI_LINK_DOWN;
    return ESP_OK;
}

int32_t bmx_embedded_wifi_initialized(void) {
    return bmx_wifi_initialized;
}

int32_t bmx_embedded_wifi_start_scan(void) {
    if (!bmx_wifi_initialized || bmx_wifi_scan_active)
        return ESP_ERR_INVALID_STATE;
    bmx_wifi_scan_active = true;
    esp_err_t result = esp_wifi_scan_start(NULL, false);
    if (result != ESP_OK) bmx_wifi_scan_active = false;
    return result;
}

static wifi_auth_mode_t bmx_wifi_authentication(uint32_t authentication) {
    switch (authentication) {
        case 0: return WIFI_AUTH_OPEN;
        case 1: return WIFI_AUTH_WPA_PSK;
        case 2: return WIFI_AUTH_WPA2_PSK;
        case 3: return WIFI_AUTH_WPA_WPA2_PSK;
        case 4: return WIFI_AUTH_WPA3_PSK;
        case 5: return WIFI_AUTH_WPA2_WPA3_PSK;
        default: return WIFI_AUTH_MAX;
    }
}

int32_t bmx_embedded_wifi_connect(const uint8_t *ssid, uint32_t ssid_length,
        const uint8_t *password, uint32_t password_length,
        uint32_t authentication) {
    if (!bmx_wifi_initialized) return ESP_ERR_INVALID_STATE;
    if (!ssid || !ssid_length || ssid_length > 32 || password_length > 63 ||
            (password_length && !password)) return ESP_ERR_INVALID_ARG;
    wifi_auth_mode_t auth = password_length
        ? bmx_wifi_authentication(authentication) : WIFI_AUTH_OPEN;
    if (auth == WIFI_AUTH_MAX) return ESP_ERR_INVALID_ARG;
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, ssid, ssid_length);
    if (password_length)
        memcpy(config.sta.password, password, password_length);
    config.sta.threshold.authmode = auth;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    esp_err_t result = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (result != ESP_OK) return result;
    bmx_wifi_explicit_disconnect = false;
    bmx_wifi_link_status = BMX_WIFI_LINK_DOWN;
    return esp_wifi_connect();
}

int32_t bmx_embedded_wifi_disconnect(void) {
    if (!bmx_wifi_initialized) return ESP_ERR_INVALID_STATE;
    bmx_wifi_explicit_disconnect = true;
    esp_err_t result = esp_wifi_disconnect();
    if (result != ESP_OK) bmx_wifi_explicit_disconnect = false;
    return result;
}

int32_t bmx_embedded_wifi_scan_active(void) { return bmx_wifi_scan_active; }
int32_t bmx_embedded_wifi_link_status(void) { return bmx_wifi_link_status; }
void bmx_embedded_wifi_service(void) {}

int32_t bmx_embedded_wifi_take_event(int32_t *kind, uint8_t *ssid,
        int32_t *ssid_length, uint8_t *bssid, int32_t *channel,
        int32_t *rssi, int32_t *security, int32_t *link_status,
        uint32_t *address, uint32_t *netmask, uint32_t *gateway) {
    if (!kind || !ssid || !ssid_length || !bssid || !channel || !rssi ||
            !security || !link_status || !address || !netmask || !gateway)
        return 0;
    taskENTER_CRITICAL();
    if (bmx_wifi_event_get == bmx_wifi_event_put) {
        taskEXIT_CRITICAL();
        return 0;
    }
    BMXESP8266WiFiEvent event =
        bmx_wifi_events[bmx_wifi_event_get & BMX_WIFI_EVENT_MASK];
    ++bmx_wifi_event_get;
    taskEXIT_CRITICAL();
    *kind = event.kind;
    *ssid_length = event.ssid_length;
    memcpy(ssid, event.ssid, sizeof(event.ssid));
    memcpy(bssid, event.bssid, sizeof(event.bssid));
    *channel = event.channel;
    *rssi = event.rssi;
    *security = event.security;
    *link_status = event.link_status;
    *address = event.address;
    *netmask = event.netmask;
    *gateway = event.gateway;
    return 1;
}

static uint32_t bmx_wifi_address(int which) {
    if (!bmx_wifi_initialized) return 0;
    tcpip_adapter_ip_info_t info;
    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &info) != ESP_OK)
        return 0;
    return which == 0 ? info.ip.addr
        : which == 1 ? info.netmask.addr : info.gw.addr;
}

uint32_t bmx_embedded_wifi_ipv4_address(void) {
    return bmx_wifi_address(0);
}

uint32_t bmx_embedded_wifi_ipv4_netmask(void) {
    return bmx_wifi_address(1);
}

uint32_t bmx_embedded_wifi_ipv4_gateway(void) {
    return bmx_wifi_address(2);
}

uint32_t bmx_embedded_wifi_dropped_events(void) {
    uint32_t dropped;
    taskENTER_CRITICAL();
    dropped = bmx_wifi_dropped;
    taskEXIT_CRITICAL();
    return dropped;
}
