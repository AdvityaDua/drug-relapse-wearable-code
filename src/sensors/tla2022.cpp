#include "tla2022.h"
#include "driver/i2c.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TLA2022_ADDR 0x49
static const char* TAG = "TLA2022";

namespace TLA2022
{
    static esp_err_t write_reg(uint8_t reg, uint16_t data) {
        // TLA2022 expects MSB first
        uint8_t write_buf[3] = { reg, (uint8_t)(data >> 8), (uint8_t)(data & 0xFF) };
        return i2c_master_write_to_device((i2c_port_t)I2C_MASTER_NUM, TLA2022_ADDR, write_buf, 3, pdMS_TO_TICKS(100));
    }

    static esp_err_t read_reg(uint8_t reg, uint16_t *data) {
        uint8_t read_buf[2] = {0};
        esp_err_t err = i2c_master_write_read_device((i2c_port_t)I2C_MASTER_NUM, TLA2022_ADDR, &reg, 1, read_buf, 2, pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            *data = (read_buf[0] << 8) | read_buf[1];
        }
        return err;
    }

    bool init() {
        // Just verify we can read the config register.
        // Default config out of factory is 0x8583.
        uint16_t config = 0;
        if (read_reg(0x01, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to communicate with TLA2022 over I2C");
            return false;
        }
        
        // Let's explicitly write the Single-Shot mode config without triggering a read yet
        // Bit 15=0, MODE=1(Single), PGA=100(+/- 0.512V), DR=000(128 SPS) -> 0x0903
        write_reg(0x01, 0x0903);
        
        return true;
    }

    int16_t readGSR() {
        // We will replicate the 8-tap FIR filter (moving sum of 8 samples)
        int32_t gsr_sum = 0;

        for (int i = 0; i < 8; i++) {
            // Trigger a single-shot conversion
            // 0x8903 = 1000 1001 0000 0011
            write_reg(0x01, 0x8903);

            // 128 SPS = 7.8ms per sample. Wait 9ms to ensure it's ready.
            vTaskDelay(pdMS_TO_TICKS(9));

            // Read conversion register
            uint16_t result = 0;
            read_reg(0x00, &result);

            // 12-bit signed, left aligned, so right shift by 4.
            int16_t val = (int16_t)result >> 4;
            gsr_sum += val;
        }

        return (int16_t)gsr_sum;
    }
}
