#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_spi.h"
#include "driver/spi.h"
#include "esp8266/spi_struct.h"
#include "esp_err.h"

extern spi_dev_t SPI1;

#define BMX_SPI_ERROR_GENERIC (-1)
#define BMX_SPI_ERROR_INVALID_ARGUMENT (-5)
#define BMX_SPI_MISO_PIN 12u
#define BMX_SPI_MOSI_PIN 13u
#define BMX_SPI_CLOCK_PIN 14u
#define BMX_SPI_CS_PIN 15u

typedef struct BMXESP8266SPIState {
    uint32_t baudrate;
    uint8_t configured;
    uint8_t initialized;
    uint8_t data_bits;
    uint8_t polarity;
    uint8_t phase;
    uint8_t bit_order;
} BMXESP8266SPIState;

static BMXESP8266SPIState bmx_esp8266_spi;

static uint32_t bmx_esp8266_spi_frequency(spi_clk_div_t divider) {
    return 80000000u / (uint32_t)divider;
}

static spi_clk_div_t bmx_esp8266_spi_divider(uint32_t baudrate) {
    static const spi_clk_div_t dividers[] = {
        SPI_80MHz_DIV, SPI_40MHz_DIV, SPI_20MHz_DIV, SPI_16MHz_DIV,
        SPI_10MHz_DIV, SPI_8MHz_DIV, SPI_5MHz_DIV, SPI_4MHz_DIV, SPI_2MHz_DIV
    };
    for (size_t index = 0; index < sizeof(dividers) / sizeof(dividers[0]); ++index) {
        if (bmx_esp8266_spi_frequency(dividers[index]) <= baudrate) return dividers[index];
    }
    return SPI_2MHz_DIV;
}

static spi_interface_t bmx_esp8266_spi_interface(void) {
    spi_interface_t interface = { .val = 0 };
    interface.cpol = bmx_esp8266_spi.polarity;
    interface.cpha = bmx_esp8266_spi.phase;
    interface.bit_tx_order = bmx_esp8266_spi.bit_order;
    interface.bit_rx_order = bmx_esp8266_spi.bit_order;
    interface.byte_tx_order = bmx_esp8266_spi.data_bits == 16 ? bmx_esp8266_spi.bit_order : 0;
    interface.byte_rx_order = bmx_esp8266_spi.data_bits == 16 ? bmx_esp8266_spi.bit_order : 0;
    interface.mosi_en = 1;
    interface.miso_en = 1;
    interface.cs_en = 0;
    return interface;
}

static int32_t bmx_esp8266_spi_apply_format(void) {
    spi_interface_t interface = bmx_esp8266_spi_interface();
    return spi_set_interface(HSPI_HOST, &interface) == ESP_OK;
}

int32_t bmx_embedded_spi_controller_count(void) { return 1; }
int32_t bmx_embedded_spi_default_controller(void) { return 0; }
uint32_t bmx_embedded_spi_default_rx_pin(void) { return BMX_SPI_MISO_PIN; }
uint32_t bmx_embedded_spi_default_tx_pin(void) { return BMX_SPI_MOSI_PIN; }
uint32_t bmx_embedded_spi_default_sck_pin(void) { return BMX_SPI_CLOCK_PIN; }
uint32_t bmx_embedded_spi_default_csn_pin(void) { return BMX_SPI_CS_PIN; }

int32_t bmx_embedded_spi_configure_pins(int32_t controller, uint32_t rx_pin,
        uint32_t tx_pin, uint32_t sck_pin) {
    if (controller != 0 || bmx_esp8266_spi.initialized || rx_pin != BMX_SPI_MISO_PIN ||
            tx_pin != BMX_SPI_MOSI_PIN || sck_pin != BMX_SPI_CLOCK_PIN) return 0;
    bmx_esp8266_spi.configured = 1;
    return 1;
}

uint32_t bmx_embedded_spi_init(int32_t controller, uint32_t baudrate) {
    if (controller != 0 || !bmx_esp8266_spi.configured ||
            bmx_esp8266_spi.initialized || baudrate < 2000000u) return 0;
    spi_clk_div_t divider = bmx_esp8266_spi_divider(baudrate);
    bmx_esp8266_spi.data_bits = 8;
    bmx_esp8266_spi.polarity = 0;
    bmx_esp8266_spi.phase = 0;
    bmx_esp8266_spi.bit_order = SPI_BIT_ORDER_MSB_FIRST;
    spi_config_t config = {
        .interface = bmx_esp8266_spi_interface(),
        .intr_enable = { .val = 0 },
        .event_cb = NULL,
        .mode = SPI_MASTER_MODE,
        .clk_div = divider,
    };
    if (spi_init(HSPI_HOST, &config) != ESP_OK) return 0;
    bmx_esp8266_spi.initialized = 1;
    bmx_esp8266_spi.baudrate = bmx_esp8266_spi_frequency(divider);
    return bmx_esp8266_spi.baudrate;
}

int32_t bmx_embedded_spi_deinit(int32_t controller) {
    if (controller != 0 || !bmx_esp8266_spi.initialized ||
            spi_deinit(HSPI_HOST) != ESP_OK) return 0;
    bmx_esp8266_spi.initialized = 0;
    bmx_esp8266_spi.baudrate = 0;
    return 1;
}

uint32_t bmx_embedded_spi_set_baudrate(int32_t controller, uint32_t baudrate) {
    if (controller != 0 || !bmx_esp8266_spi.initialized || baudrate < 2000000u) return 0;
    spi_clk_div_t divider = bmx_esp8266_spi_divider(baudrate);
    if (spi_set_clk_div(HSPI_HOST, &divider) != ESP_OK) return 0;
    bmx_esp8266_spi.baudrate = bmx_esp8266_spi_frequency(divider);
    return bmx_esp8266_spi.baudrate;
}

uint32_t bmx_embedded_spi_get_baudrate(int32_t controller) {
    return controller == 0 && bmx_esp8266_spi.initialized ? bmx_esp8266_spi.baudrate : 0;
}

int32_t bmx_embedded_spi_set_format(int32_t controller, uint32_t data_bits,
        uint32_t polarity, uint32_t phase, uint32_t bit_order) {
    if (controller != 0 || !bmx_esp8266_spi.initialized ||
            (data_bits != 8u && data_bits != 16u) || polarity > 1u ||
            phase > 1u || bit_order > 1u) return 0;
    BMXESP8266SPIState previous = bmx_esp8266_spi;
    bmx_esp8266_spi.data_bits = (uint8_t)data_bits;
    bmx_esp8266_spi.polarity = (uint8_t)polarity;
    bmx_esp8266_spi.phase = (uint8_t)phase;
    bmx_esp8266_spi.bit_order = (uint8_t)bit_order;
    if (!bmx_esp8266_spi_apply_format()) {
        bmx_esp8266_spi = previous;
        (void)bmx_esp8266_spi_apply_format();
        return 0;
    }
    return 1;
}

static int32_t bmx_esp8266_spi_transfer(const void *source, void *destination,
        int32_t byte_length) {
    if (!bmx_esp8266_spi.initialized || byte_length < 0 ||
            (byte_length && !source && !destination)) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    const uint8_t *input = source;
    uint8_t *output = destination;
    int32_t offset = 0;
    while (offset < byte_length) {
        int32_t chunk = byte_length - offset;
        if (chunk > 64) chunk = 64;
        uint32_t tx_words[16] = {0};
        uint32_t rx_words[16] = {0};
        if (input) memcpy(tx_words, input + offset, (size_t)chunk);
        spi_trans_t transfer = {0};
        if (input) {
            transfer.mosi = tx_words;
            transfer.bits.mosi = (uint32_t)chunk * 8u;
        }
        /* The legacy driver models MOSI and MISO as consecutive half-duplex
           phases when both lengths are populated. HSPI itself is configured
           for duplex operation, so run the MOSI phase and collect the receive
           words directly after the hardware finishes that same phase. */
        if (spi_trans(HSPI_HOST, &transfer) != ESP_OK) return BMX_SPI_ERROR_GENERIC;
        while (SPI1.cmd.usr) { }
        if (output) {
            for (int32_t word = 0; word < (chunk + 3) / 4; ++word)
                rx_words[word] = SPI1.data_buf[word];
            memcpy(output + offset, rx_words, (size_t)chunk);
        }
        offset += chunk;
    }
    return byte_length;
}

int32_t bmx_embedded_spi_write_read_blocking(int32_t controller, void *source,
        void *destination, int32_t length) {
    if (controller != 0 || bmx_esp8266_spi.data_bits != 8 || length < 0 ||
            (length && (!source || !destination))) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    return bmx_esp8266_spi_transfer(source, destination, length);
}
int32_t bmx_embedded_spi_write_blocking(int32_t controller, void *source, int32_t length) {
    if (controller != 0 || bmx_esp8266_spi.data_bits != 8 || length < 0 ||
            (length && !source)) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    return bmx_esp8266_spi_transfer(source, NULL, length);
}
int32_t bmx_embedded_spi_read_blocking(int32_t controller, uint32_t repeated_data,
        void *destination, int32_t length) {
    if (controller != 0 || bmx_esp8266_spi.data_bits != 8 || repeated_data > 0xffu ||
            length < 0 || (length && !destination)) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    uint8_t repeated[64];
    memset(repeated, (uint8_t)repeated_data, sizeof(repeated));
    uint8_t *output = destination;
    int32_t offset = 0;
    while (offset < length) {
        int32_t chunk = length - offset;
        if (chunk > 64) chunk = 64;
        int32_t result = bmx_esp8266_spi_transfer(repeated, output + offset, chunk);
        if (result < 0) return result;
        offset += chunk;
    }
    return length;
}
int32_t bmx_embedded_spi_write16_read16_blocking(int32_t controller, uint16_t *source,
        uint16_t *destination, int32_t length) {
    if (controller != 0 || bmx_esp8266_spi.data_bits != 16 || length < 0 ||
            (length && (!source || !destination)) || length > INT32_MAX / 2)
        return BMX_SPI_ERROR_INVALID_ARGUMENT;
    int32_t result = bmx_esp8266_spi_transfer(source, destination, length * 2);
    return result < 0 ? result : length;
}
int32_t bmx_embedded_spi_write16_blocking(int32_t controller, uint16_t *source,
        int32_t length) {
    if (controller != 0 || bmx_esp8266_spi.data_bits != 16 || length < 0 ||
            (length && !source) || length > INT32_MAX / 2) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    int32_t result = bmx_esp8266_spi_transfer(source, NULL, length * 2);
    return result < 0 ? result : length;
}
int32_t bmx_embedded_spi_read16_blocking(int32_t controller, uint32_t repeated_data,
        uint16_t *destination, int32_t length) {
    if (controller != 0 || bmx_esp8266_spi.data_bits != 16 || repeated_data > 0xffffu ||
            length < 0 || (length && !destination)) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    uint16_t repeated[32];
    for (size_t index = 0; index < 32; ++index) repeated[index] = (uint16_t)repeated_data;
    int32_t offset = 0;
    while (offset < length) {
        int32_t chunk = length - offset;
        if (chunk > 32) chunk = 32;
        int32_t result = bmx_esp8266_spi_transfer(repeated, destination + offset, chunk * 2);
        if (result < 0) return result;
        offset += chunk;
    }
    return length;
}
