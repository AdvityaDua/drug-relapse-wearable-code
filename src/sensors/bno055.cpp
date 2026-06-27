#include "bno055.h"
#include "driver/i2c.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BNO055_ADDR 0x28
static const char* TAG = "BNO055";

namespace BNO055
{
    static esp_err_t write_reg(uint8_t reg, uint8_t data) {
        uint8_t write_buf[2] = {reg, data};
        return i2c_master_write_to_device((i2c_port_t)I2C_MASTER_NUM, BNO055_ADDR, write_buf, 2, pdMS_TO_TICKS(100));
    }

    static esp_err_t read_len(uint8_t reg, uint8_t *data, size_t len) {
        return i2c_master_write_read_device((i2c_port_t)I2C_MASTER_NUM, BNO055_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
    }

    static esp_err_t read_reg(uint8_t reg, uint8_t *data) {
        return read_len(reg, data, 1);
    }

    bool init() {
        uint8_t id = 0;
        
        // Wait a tiny bit for the sensor to fully boot after power up
        vTaskDelay(pdMS_TO_TICKS(10));
        
        if (read_reg(0x00, &id) != ESP_OK) { // BNO055_REG_CHIP_ID
            ESP_LOGE(TAG, "Failed to communicate with BNO055 over I2C");
            return false;
        }
        
        if (id != 0xA0) {
            ESP_LOGE(TAG, "BNO055 Chip ID mismatch! Expected 0xA0, got 0x%02X", id);
            return false;
        }

        // Switch to CONFIG mode
        write_reg(0x3D, 0x00);
        vTaskDelay(pdMS_TO_TICKS(25));

        // Soft Reset
        write_reg(0x3F, 0x20); // SYS_TRIGGER: RST_SYS
        vTaskDelay(pdMS_TO_TICKS(650)); // Wait for reset to finish

        // Verify it came back
        read_reg(0x00, &id);
        if (id != 0xA0) {
            ESP_LOGE(TAG, "BNO055 did not recover from reset!");
            return false;
        }

        // Set to Normal power mode
        write_reg(0x3E, 0x00);
        vTaskDelay(pdMS_TO_TICKS(10));

        // Page ID 0
        write_reg(0x07, 0x00);
        
        // Trigger external crystal
        write_reg(0x3F, 0x80);
        vTaskDelay(pdMS_TO_TICKS(10));

        // Set to NDOF (Nine Degrees of Freedom Sensor Fusion) mode (0x0C)
        write_reg(0x3D, 0x0C);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        return true;
    }

    void readAll(BNO055_Data* out) {
        // Registers span from 0x08 (ACCEL_X_LSB) to 0x35 (CALIB_STAT)
        // Length = 0x35 - 0x08 + 1 = 46 bytes
        uint8_t buffer[46];
        
        esp_err_t err = read_len(0x08, buffer, 46);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to burst read BNO055 data");
            return;
        }
        
        // Acceleration (1 m/s2 = 100 lsb)
        int16_t ax = (buffer[1] << 8) | buffer[0];
        int16_t ay = (buffer[3] << 8) | buffer[2];
        int16_t az = (buffer[5] << 8) | buffer[4];
        out->accelX = ax / 100.0f;
        out->accelY = ay / 100.0f;
        out->accelZ = az / 100.0f;

        // Magnetometer (1 uT = 16 lsb)
        int16_t mx = (buffer[7] << 8) | buffer[6];
        int16_t my = (buffer[9] << 8) | buffer[8];
        int16_t mz = (buffer[11] << 8) | buffer[10];
        out->magX = mx / 16.0f;
        out->magY = my / 16.0f;
        out->magZ = mz / 16.0f;

        // Gyroscope (1 dps = 16 lsb)
        int16_t gx = (buffer[13] << 8) | buffer[12];
        int16_t gy = (buffer[15] << 8) | buffer[14];
        int16_t gz = (buffer[17] << 8) | buffer[16];
        out->gyroX = gx / 16.0f;
        out->gyroY = gy / 16.0f;
        out->gyroZ = gz / 16.0f;

        // Euler Angles (1 degree = 16 lsb)
        int16_t eh = (buffer[19] << 8) | buffer[18];
        int16_t er = (buffer[21] << 8) | buffer[20];
        int16_t ep = (buffer[23] << 8) | buffer[22];
        out->eulerHeading = eh / 16.0f;
        out->eulerRoll = er / 16.0f;
        out->eulerPitch = ep / 16.0f;

        // Quaternions (1 quaternion unit = 2^14 lsb)
        int16_t qw = (buffer[25] << 8) | buffer[24];
        int16_t qx = (buffer[27] << 8) | buffer[26];
        int16_t qy = (buffer[29] << 8) | buffer[28];
        int16_t qz = (buffer[31] << 8) | buffer[30];
        out->quatW = qw / 16384.0f;
        out->quatX = qx / 16384.0f;
        out->quatY = qy / 16384.0f;
        out->quatZ = qz / 16384.0f;

        // Linear Acceleration (1 m/s2 = 100 lsb)
        int16_t lx = (buffer[33] << 8) | buffer[32];
        int16_t ly = (buffer[35] << 8) | buffer[34];
        int16_t lz = (buffer[37] << 8) | buffer[36];
        out->linearAccelX = lx / 100.0f;
        out->linearAccelY = ly / 100.0f;
        out->linearAccelZ = lz / 100.0f;

        // Gravity (1 m/s2 = 100 lsb)
        int16_t grx = (buffer[39] << 8) | buffer[38];
        int16_t gry = (buffer[41] << 8) | buffer[40];
        int16_t grz = (buffer[43] << 8) | buffer[42];
        out->gravityX = grx / 100.0f;
        out->gravityY = gry / 100.0f;
        out->gravityZ = grz / 100.0f;

        // Temperature (1 Degree C = 1 lsb)
        out->temp = (int8_t)buffer[44];

        // Calibration Status (0 = uncalibrated, 3 = fully calibrated)
        uint8_t calib = buffer[45];
        out->calibSys = (calib >> 6) & 0x03;
        out->calibGyro = (calib >> 4) & 0x03;
        out->calibAccel = (calib >> 2) & 0x03;
        out->calibMag = calib & 0x03;
    }

    bool getCalibrationProfile(uint8_t* profileData) {
        if (read_len(0x55, profileData, 22) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read BNO055 calibration profile");
            return false;
        }
        return true;
    }

    bool setCalibrationProfile(const uint8_t* profileData) {
        // Switch to CONFIG mode
        write_reg(0x3D, 0x00);
        vTaskDelay(pdMS_TO_TICKS(25));

        // Burst write 22 bytes to 0x55
        // i2c_master_write_to_device requires the register address as the first byte
        uint8_t write_buf[23];
        write_buf[0] = 0x55;
        memcpy(&write_buf[1], profileData, 22);

        esp_err_t err = i2c_master_write_to_device((i2c_port_t)I2C_MASTER_NUM, BNO055_ADDR, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
        
        // Restore NDOF mode
        write_reg(0x3D, 0x0C);
        vTaskDelay(pdMS_TO_TICKS(20));

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write BNO055 calibration profile");
            return false;
        }
        
        ESP_LOGI(TAG, "Successfully injected BNO055 calibration profile");
        return true;
    }
}
