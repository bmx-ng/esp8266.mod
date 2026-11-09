#include <limits.h>
#include <stdint.h>

#include "blitzmax/embedded_i2c.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#define BMX_I2C_CONTROLLER 0
#define BMX_I2C_NOMINAL_BAUDRATE 100000u
#define BMX_I2C_DEFAULT_SDA 4u
#define BMX_I2C_DEFAULT_SCL 5u
#define BMX_I2C_ERROR_GENERIC (-1)
#define BMX_I2C_ERROR_INVALID_ARGUMENT (-5)

typedef struct BMXESP8266I2CState {
    uint32_t sda_pin;
    uint32_t scl_pin;
    uint8_t configured;
    uint8_t initialized;
    uint8_t pull_ups;
} BMXESP8266I2CState;

static BMXESP8266I2CState bmx_esp8266_i2c;

static int32_t bmx_esp8266_i2c_valid_controller(int32_t controller) {
    return controller == BMX_I2C_CONTROLLER;
}

static int32_t bmx_esp8266_i2c_valid_pin(uint32_t pin) {
    return pin <= 16u && (pin < 6u || pin > 11u);
}

static int32_t bmx_esp8266_i2c_valid_transfer(int32_t controller,
        uint32_t address, const void *data, int32_t length) {
    return bmx_esp8266_i2c_valid_controller(controller) &&
        bmx_esp8266_i2c.initialized && address <= 0x7fu && data && length > 0;
}

static int32_t bmx_esp8266_i2c_result(esp_err_t error, int32_t length) {
    if (error == ESP_OK) return length;
    if (error == ESP_ERR_INVALID_ARG || error == ESP_ERR_INVALID_STATE)
        return BMX_I2C_ERROR_INVALID_ARGUMENT;
    return BMX_I2C_ERROR_GENERIC;
}

static TickType_t bmx_esp8266_i2c_ticks(uint32_t timeout_us) {
    uint64_t milliseconds = ((uint64_t)timeout_us + 999u) / 1000u;
    if (!milliseconds) milliseconds = 1u;
    if (milliseconds > (uint64_t)portMAX_DELAY) return portMAX_DELAY;
    TickType_t ticks = (TickType_t)(milliseconds / portTICK_PERIOD_MS);
    return ticks ? ticks : 1;
}

int32_t bmx_embedded_i2c_controller_count(void) { return 1; }
int32_t bmx_embedded_i2c_default_controller(void) { return BMX_I2C_CONTROLLER; }
uint32_t bmx_embedded_i2c_default_sda_pin(void) { return BMX_I2C_DEFAULT_SDA; }
uint32_t bmx_embedded_i2c_default_scl_pin(void) { return BMX_I2C_DEFAULT_SCL; }

int32_t bmx_embedded_i2c_configure_pins(int32_t controller, uint32_t sda_pin,
        uint32_t scl_pin, int32_t pull_ups) {
    if (!bmx_esp8266_i2c_valid_controller(controller) || bmx_esp8266_i2c.initialized ||
            sda_pin == scl_pin || !bmx_esp8266_i2c_valid_pin(sda_pin) ||
            !bmx_esp8266_i2c_valid_pin(scl_pin) ||
            (pull_ups && (sda_pin == 16u || scl_pin == 16u))) return 0;
    bmx_esp8266_i2c.sda_pin = sda_pin;
    bmx_esp8266_i2c.scl_pin = scl_pin;
    bmx_esp8266_i2c.pull_ups = pull_ups != 0;
    bmx_esp8266_i2c.configured = 1;
    return 1;
}

uint32_t bmx_embedded_i2c_init(int32_t controller, uint32_t baudrate) {
    if (!bmx_esp8266_i2c_valid_controller(controller) || !bmx_esp8266_i2c.configured ||
            bmx_esp8266_i2c.initialized || !baudrate ||
            baudrate > BMX_I2C_NOMINAL_BAUDRATE) return 0;
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (int)bmx_esp8266_i2c.sda_pin,
        .sda_pullup_en = bmx_esp8266_i2c.pull_ups,
        .scl_io_num = (int)bmx_esp8266_i2c.scl_pin,
        .scl_pullup_en = bmx_esp8266_i2c.pull_ups,
        .clk_stretch_tick = 300,
    };
    if (i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER) != ESP_OK) return 0;
    if (i2c_param_config(I2C_NUM_0, &config) != ESP_OK) {
        (void)i2c_driver_delete(I2C_NUM_0);
        return 0;
    }
    bmx_esp8266_i2c.initialized = 1;
    return BMX_I2C_NOMINAL_BAUDRATE;
}

int32_t bmx_embedded_i2c_deinit(int32_t controller) {
    if (!bmx_esp8266_i2c_valid_controller(controller) || !bmx_esp8266_i2c.initialized ||
            i2c_driver_delete(I2C_NUM_0) != ESP_OK) return 0;
    bmx_esp8266_i2c.initialized = 0;
    return 1;
}

uint32_t bmx_embedded_i2c_set_baudrate(int32_t controller, uint32_t baudrate) {
    if (!bmx_esp8266_i2c_valid_controller(controller) || !bmx_esp8266_i2c.initialized ||
            !baudrate || baudrate > BMX_I2C_NOMINAL_BAUDRATE) return 0;
    return BMX_I2C_NOMINAL_BAUDRATE;
}

static int32_t bmx_esp8266_i2c_write(int32_t controller, uint32_t address,
        void *data, int32_t length, TickType_t ticks) {
    if (!bmx_esp8266_i2c_valid_transfer(controller, address, data, length))
        return BMX_I2C_ERROR_INVALID_ARGUMENT;
    i2c_cmd_handle_t command = i2c_cmd_link_create();
    if (!command) return BMX_I2C_ERROR_GENERIC;
    esp_err_t error = i2c_master_start(command);
    if (error == ESP_OK) error = i2c_master_write_byte(command,
        (uint8_t)(address << 1u) | I2C_MASTER_WRITE, true);
    if (error == ESP_OK) error = i2c_master_write(command, data, (size_t)length, true);
    if (error == ESP_OK) error = i2c_master_stop(command);
    if (error == ESP_OK) error = i2c_master_cmd_begin(I2C_NUM_0, command, ticks);
    i2c_cmd_link_delete(command);
    return bmx_esp8266_i2c_result(error, length);
}

static int32_t bmx_esp8266_i2c_read(int32_t controller, uint32_t address,
        void *data, int32_t length, TickType_t ticks) {
    if (!bmx_esp8266_i2c_valid_transfer(controller, address, data, length))
        return BMX_I2C_ERROR_INVALID_ARGUMENT;
    i2c_cmd_handle_t command = i2c_cmd_link_create();
    if (!command) return BMX_I2C_ERROR_GENERIC;
    esp_err_t error = i2c_master_start(command);
    if (error == ESP_OK) error = i2c_master_write_byte(command,
        (uint8_t)(address << 1u) | I2C_MASTER_READ, true);
    if (error == ESP_OK && length > 1)
        error = i2c_master_read(command, data, (size_t)length - 1u, I2C_MASTER_ACK);
    if (error == ESP_OK)
        error = i2c_master_read_byte(command, (uint8_t *)data + length - 1, I2C_MASTER_NACK);
    if (error == ESP_OK) error = i2c_master_stop(command);
    if (error == ESP_OK) error = i2c_master_cmd_begin(I2C_NUM_0, command, ticks);
    i2c_cmd_link_delete(command);
    return bmx_esp8266_i2c_result(error, length);
}

static int32_t bmx_esp8266_i2c_write_read(int32_t controller, uint32_t address,
        void *write_data, int32_t write_length, void *read_data, int32_t read_length,
        TickType_t ticks) {
    if (!bmx_esp8266_i2c_valid_transfer(controller, address, write_data, write_length) ||
            !read_data || read_length <= 0) return BMX_I2C_ERROR_INVALID_ARGUMENT;
    i2c_cmd_handle_t command = i2c_cmd_link_create();
    if (!command) return BMX_I2C_ERROR_GENERIC;
    esp_err_t error = i2c_master_start(command);
    if (error == ESP_OK) error = i2c_master_write_byte(command,
        (uint8_t)(address << 1u) | I2C_MASTER_WRITE, true);
    if (error == ESP_OK)
        error = i2c_master_write(command, write_data, (size_t)write_length, true);
    if (error == ESP_OK) error = i2c_master_start(command);
    if (error == ESP_OK) error = i2c_master_write_byte(command,
        (uint8_t)(address << 1u) | I2C_MASTER_READ, true);
    if (error == ESP_OK && read_length > 1)
        error = i2c_master_read(command, read_data, (size_t)read_length - 1u, I2C_MASTER_ACK);
    if (error == ESP_OK) error = i2c_master_read_byte(command,
        (uint8_t *)read_data + read_length - 1, I2C_MASTER_NACK);
    if (error == ESP_OK) error = i2c_master_stop(command);
    if (error == ESP_OK) error = i2c_master_cmd_begin(I2C_NUM_0, command, ticks);
    i2c_cmd_link_delete(command);
    return bmx_esp8266_i2c_result(error, read_length);
}

int32_t bmx_embedded_i2c_write_blocking(int32_t controller, uint32_t address,
        void *data, int32_t length) {
    return bmx_esp8266_i2c_write(controller, address, data, length, portMAX_DELAY);
}
int32_t bmx_embedded_i2c_read_blocking(int32_t controller, uint32_t address,
        void *data, int32_t length) {
    return bmx_esp8266_i2c_read(controller, address, data, length, portMAX_DELAY);
}
int32_t bmx_embedded_i2c_write_timeout_us(int32_t controller, uint32_t address,
        void *data, int32_t length, uint32_t timeout_us) {
    return bmx_esp8266_i2c_write(controller, address, data, length,
        bmx_esp8266_i2c_ticks(timeout_us));
}
int32_t bmx_embedded_i2c_read_timeout_us(int32_t controller, uint32_t address,
        void *data, int32_t length, uint32_t timeout_us) {
    return bmx_esp8266_i2c_read(controller, address, data, length,
        bmx_esp8266_i2c_ticks(timeout_us));
}
int32_t bmx_embedded_i2c_write_read_blocking(int32_t controller, uint32_t address,
        void *write_data, int32_t write_length, void *read_data, int32_t read_length) {
    return bmx_esp8266_i2c_write_read(controller, address, write_data, write_length,
        read_data, read_length, portMAX_DELAY);
}
int32_t bmx_embedded_i2c_write_read_timeout_us(int32_t controller, uint32_t address,
        void *write_data, int32_t write_length, void *read_data, int32_t read_length,
        uint32_t timeout_us) {
    return bmx_esp8266_i2c_write_read(controller, address, write_data, write_length,
        read_data, read_length, bmx_esp8266_i2c_ticks(timeout_us));
}
