#include "max30102.h"
#include "algorithm.h"
#include "driver/i2c.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX30102_ADDR 0x57
static const char* TAG = "MAX30102";

namespace MAX30102
{
    static esp_err_t write_reg(uint8_t reg, uint8_t data) {
        uint8_t write_buf[2] = {reg, data};
        return i2c_master_write_to_device((i2c_port_t)I2C_MASTER_NUM, MAX30102_ADDR, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
    }

    static esp_err_t read_reg(uint8_t reg, uint8_t *data) {
        return i2c_master_write_read_device((i2c_port_t)I2C_MASTER_NUM, MAX30102_ADDR, &reg, 1, data, 1, pdMS_TO_TICKS(100));
    }

    bool init() {
        // Soft Reset
        write_reg(0x09, 0x40);
        vTaskDelay(pdMS_TO_TICKS(100)); 

        // FIFO configuration: sample average = 1 (0x00), rollover enabled, almost full = 15
        write_reg(0x08, 0x0F); 

        // Mode configuration: SpO2 mode (Red + IR)
        write_reg(0x09, 0x03); 

        // SpO2 configuration: ADC range=4096, Sample Rate=100Hz, LED Pulse Width=411us
        write_reg(0x0A, 0x27); 

        // LED Pulse Amplitudes (around 7mA)
        write_reg(0x0C, 0x24); 
        write_reg(0x0D, 0x24); 
        
        // Clear FIFO pointers
        write_reg(0x04, 0x00);
        write_reg(0x05, 0x00);
        write_reg(0x06, 0x00);

        return true;
    }

    MAX30102_Data readAndCalculate() {
        MAX30102_Data outData = {0, 0, 0, 0};
        uint32_t irBuffer[100];
        uint32_t redBuffer[100];
        
        ESP_LOGI(TAG, "Collecting 100 samples (~1 second of data)...");
        
        // Clear FIFO to start fresh reading
        write_reg(0x04, 0x00);
        write_reg(0x05, 0x00);
        write_reg(0x06, 0x00);

        int sampleCount = 0;
        uint8_t reg = 0x07; // FIFO Data register
        uint8_t rx_buf[6];

        while (sampleCount < 100) {
            uint8_t wr_ptr = 0, rd_ptr = 0;
            read_reg(0x04, &wr_ptr);
            read_reg(0x06, &rd_ptr);
            
            int samplesAvailable = wr_ptr - rd_ptr;
            if (samplesAvailable < 0) samplesAvailable += 32; // Rollover handling

            for (int i = 0; i < samplesAvailable && sampleCount < 100; i++) {
                // Read 6 bytes from FIFO (3 Red, 3 IR)
                i2c_master_write_read_device((i2c_port_t)I2C_MASTER_NUM, MAX30102_ADDR, &reg, 1, rx_buf, 6, pdMS_TO_TICKS(100));
                
                uint32_t red = ((uint32_t)rx_buf[0] << 16) | ((uint32_t)rx_buf[1] << 8) | rx_buf[2];
                uint32_t ir = ((uint32_t)rx_buf[3] << 16) | ((uint32_t)rx_buf[4] << 8) | rx_buf[5];
                
                // Data is left-justified, only lowest 18 bits are valid
                redBuffer[sampleCount] = red & 0x03FFFF;
                irBuffer[sampleCount] = ir & 0x03FFFF;
                sampleCount++;
            }
            // Sleep to let the FIFO fill (at 100Hz, 1 sample = 10ms)
            vTaskDelay(pdMS_TO_TICKS(10)); 
        }

        int32_t spo2;
        int8_t validSPO2;
        int32_t heartRate;
        int8_t validHR;
        
        ESP_LOGI(TAG, "Calculating Heart Rate & SpO2 using Maxim Algorithm...");
        maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, &spo2, &validSPO2, &heartRate, &validHR);
        
        if (validHR || validSPO2) {
            ESP_LOGI(TAG, "SUCCESS -> HR: %ld bpm, SpO2: %ld %%", heartRate, spo2);
        } else {
            ESP_LOGW(TAG, "FAILED -> Could not detect finger/pulse (HR: %ld, SpO2: %ld)", heartRate, spo2);
        }

        outData.heartRate = heartRate;
        outData.validHR = validHR;
        outData.spo2 = spo2;
        outData.validSPO2 = validSPO2;
        
        return outData;
    }
}
