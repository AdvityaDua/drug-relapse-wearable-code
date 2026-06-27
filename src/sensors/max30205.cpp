#include "max30205.h"
#include "driver/i2c.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Set to 0x48 to avoid conflict with TinyGSR at 0x49
#define MAX30205_ADDR 0x48
static const char* TAG = "MAX30205";

#define REG_TEMPERATURE   0x00
#define REG_CONFIGURATION 0x01

#define RESOLUTION        0.00390625f

namespace MAX30205
{
    static esp_err_t write_reg(uint8_t reg, uint8_t data) {
        uint8_t write_buf[2] = {reg, data};
        return i2c_master_write_to_device((i2c_port_t)I2C_MASTER_NUM, MAX30205_ADDR, write_buf, 2, pdMS_TO_TICKS(100));
    }

    static esp_err_t read_reg(uint8_t reg, uint8_t *data) {
        return i2c_master_write_read_device((i2c_port_t)I2C_MASTER_NUM, MAX30205_ADDR, &reg, 1, data, 1, pdMS_TO_TICKS(100));
    }

    static esp_err_t read_len(uint8_t reg, uint8_t *data, size_t len) {
        return i2c_master_write_read_device((i2c_port_t)I2C_MASTER_NUM, MAX30205_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
    }

    bool init() {
        // Check if device responds
        uint8_t config = 0;
        if (read_reg(REG_CONFIGURATION, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to communicate with MAX30205 over I2C");
            return false;
        }

        // Put the sensor into Shutdown Mode immediately to save power
        // Bit 0 = SHUTDOWN
        write_reg(REG_CONFIGURATION, config | 0x01);
        
        return true;
    }

    float readTemperature() {
        uint8_t config = 0;
        read_reg(REG_CONFIGURATION, &config);

        // To trigger a One-Shot conversion, set Bit 7 (ONE-SHOT) while still in Shutdown mode (Bit 0)
        write_reg(REG_CONFIGURATION, config | 0x81);

        // MAX30205 max conversion time is ~50ms
        vTaskDelay(pdMS_TO_TICKS(55));

        // Read the 2-byte temperature register
        uint8_t raw[2] = {0};
        read_len(REG_TEMPERATURE, raw, 2);

        // 16-bit signed, left aligned logic
        int16_t value = (int16_t)((raw[0] << 8) | raw[1]);
        return value * RESOLUTION;
    }
}
