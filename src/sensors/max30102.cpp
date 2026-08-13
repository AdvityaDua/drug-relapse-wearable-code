#include "max30102.h"
#include "driver/i2c.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// DFRobot BloodOxygen_S module — MCU sits at 0x57 (same as raw MAX30102)
// but uses its own register protocol, NOT the standard MAX30102 register map.
#define MAX30102_ADDR 0x57
static const char* TAG = "MAX30102";

namespace MAX30102
{
    // Write len bytes of data to a DFRobot register
    static esp_err_t write_reg(uint8_t reg, const uint8_t* data, size_t len) {
        uint8_t buf[len + 1];
        buf[0] = reg;
        for (size_t i = 0; i < len; i++) {
            buf[i + 1] = data[i];
        }
        return i2c_master_write_to_device((i2c_port_t)I2C_MASTER_NUM, MAX30102_ADDR, buf, len + 1, pdMS_TO_TICKS(100));
    }

    // Read len bytes from a DFRobot register
    static esp_err_t read_reg(uint8_t reg, uint8_t* data, size_t len) {
        return i2c_master_write_read_device((i2c_port_t)I2C_MASTER_NUM, MAX30102_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(200));
    }

    bool init() {
        ESP_LOGI(TAG, "Initializing DFRobot BloodOxygen_S module at I2C 0x57...");

        // Send sensorStartCollect command: write [0x00, 0x01] to register 0x20
        // This tells the onboard MCU to power on the MAX30102 LED and start sampling
        uint8_t startCmd[2] = {0x00, 0x01};
        esp_err_t err = write_reg(0x20, startCmd, 2);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send startCollect command: %s", esp_err_to_name(err));
            return false;
        }

        ESP_LOGI(TAG, "Sent startCollect command. LED should turn ON now.");
        
        // Give the module time to start its internal sampling
        vTaskDelay(pdMS_TO_TICKS(500));

        return true;
    }

    void powerOff() {
        // Send sensorEndCollect command: write [0x00, 0x02] to register 0x20
        uint8_t stopCmd[2] = {0x00, 0x02};
        write_reg(0x20, stopCmd, 2);
        ESP_LOGI(TAG, "Sent endCollect command. LED should turn OFF.");
    }

    MAX30102_Data readAndCalculate() {
        MAX30102_Data outData = {0, 0, 0, 0};

        // The DFRobot module's MCU handles all sampling and algorithm internally.
        // We just need to wait a bit and then read the pre-calculated results.
        // Wait ~2 seconds to let the module accumulate enough pulse data
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Read 8 bytes from register 0x0C — this contains SPO2 and Heart Rate
        // Format (from DFRobot library):
        //   rbuf[0] = SPO2 (uint8_t, 0 means invalid)
        //   rbuf[1] = (unused/reserved)
        //   rbuf[2..5] = Heartbeat (uint32_t big-endian, 0 means invalid)
        //   rbuf[6..7] = (additional data)
        uint8_t rbuf[8] = {0};
        esp_err_t err = read_reg(0x0C, rbuf, 8);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read HR/SpO2 data: %s", esp_err_to_name(err));
            return outData;
        }

        int32_t spo2 = rbuf[0];
        int32_t heartRate = ((uint32_t)rbuf[2] << 24) | ((uint32_t)rbuf[3] << 16) | 
                            ((uint32_t)rbuf[4] << 8)  | ((uint32_t)rbuf[5]);

        ESP_LOGI(TAG, "Raw data: SPO2=%ld, HR=%ld (bytes: %02X %02X %02X %02X %02X %02X %02X %02X)",
                 spo2, heartRate, rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);

        // The DFRobot module returns 0 for invalid readings (no finger detected)
        if (spo2 > 0 && spo2 <= 100) {
            outData.spo2 = spo2;
            outData.validSPO2 = 1;
        } else {
            outData.validSPO2 = 0;
            ESP_LOGW(TAG, "SpO2 invalid (no finger?)");
        }

        if (heartRate > 0 && heartRate < 300) {
            outData.heartRate = heartRate;
            outData.validHR = 1;
        } else {
            outData.validHR = 0;
            ESP_LOGW(TAG, "Heart Rate invalid (no finger?)");
        }

        if (outData.validHR || outData.validSPO2) {
            ESP_LOGI(TAG, "SUCCESS -> HR: %ld bpm, SpO2: %ld %%", outData.heartRate, outData.spo2);
        }

        return outData;
    }
}
