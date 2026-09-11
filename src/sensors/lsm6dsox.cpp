#include "lsm6dsox.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "config.h"

static const char* TAG = "LSM6DSOX";

#define LSM6DSOX_I2C_ADDR  0x6A // Default I2C Address (can be 0x6B)

#define LSM6DSOX_WHO_AM_I  0x0F
#define LSM6DSOX_CTRL1_XL  0x10
#define LSM6DSOX_CTRL2_G   0x11
#define LSM6DSOX_CTRL3_C   0x12
#define LSM6DSOX_OUT_TEMP_L 0x20

namespace LSM6DSOX
{
    static uint8_t deviceAddress = LSM6DSOX_I2C_ADDR;

    static esp_err_t writeByte(uint8_t reg, uint8_t data)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (deviceAddress << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg, true);
        i2c_master_write_byte(cmd, data, true);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin((i2c_port_t)I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        return err;
    }

    static esp_err_t readBytes(uint8_t reg, uint8_t* data, size_t length)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (deviceAddress << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (deviceAddress << 1) | I2C_MASTER_READ, true);
        if (length > 1) {
            i2c_master_read(cmd, data, length - 1, I2C_MASTER_ACK);
        }
        i2c_master_read_byte(cmd, data + length - 1, I2C_MASTER_NACK);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin((i2c_port_t)I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        return err;
    }

    bool init()
    {
        // Auto-detect I2C address
        uint8_t who_am_i = 0;
        deviceAddress = 0x6A;
        if (readBytes(LSM6DSOX_WHO_AM_I, &who_am_i, 1) != ESP_OK || who_am_i != 0x6C) {
            deviceAddress = 0x6B;
            if (readBytes(LSM6DSOX_WHO_AM_I, &who_am_i, 1) != ESP_OK || who_am_i != 0x6C) {
                ESP_LOGE(TAG, "LSM6DSOX not found at 0x6A or 0x6B. WHO_AM_I = 0x%02X", who_am_i);
                return false;
            }
        }

        ESP_LOGI(TAG, "Found LSM6DSOX at 0x%02X", deviceAddress);

        // Reset the device (Software reset)
        writeByte(LSM6DSOX_CTRL3_C, 0x01);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Configure Block Data Update (BDU) and IF_INC
        writeByte(LSM6DSOX_CTRL3_C, 0x44);

        // Configure Accelerometer: 104Hz, +/- 2g
        writeByte(LSM6DSOX_CTRL1_XL, 0x40);

        // Configure Gyroscope: 104Hz, +/- 250dps
        writeByte(LSM6DSOX_CTRL2_G, 0x40);

        return true;
    }

    void readAll(LSM6DSOX_Data* out)
    {
        uint8_t buffer[14];
        if (readBytes(LSM6DSOX_OUT_TEMP_L, buffer, 14) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read data");
            return;
        }

        int16_t rawTemp = (buffer[1] << 8) | buffer[0];
        int16_t rawGyroX = (buffer[3] << 8) | buffer[2];
        int16_t rawGyroY = (buffer[5] << 8) | buffer[4];
        int16_t rawGyroZ = (buffer[7] << 8) | buffer[6];
        int16_t rawAccelX = (buffer[9] << 8) | buffer[8];
        int16_t rawAccelY = (buffer[11] << 8) | buffer[10];
        int16_t rawAccelZ = (buffer[13] << 8) | buffer[12];

        // Temperature: 256 LSB/°C, 25°C offset
        out->temp = (float)rawTemp / 256.0f + 25.0f;

        // Gyro: +/- 250dps -> 8.75 mdps/LSB
        out->gyroX = (float)rawGyroX * 0.00875f;
        out->gyroY = (float)rawGyroY * 0.00875f;
        out->gyroZ = (float)rawGyroZ * 0.00875f;

        // Accel: +/- 2g -> 0.061 mg/LSB -> to m/s^2 ( * 0.001 * 9.80665 )
        const float accelScale = 0.061f * 0.001f * 9.80665f;
        out->accelX = (float)rawAccelX * accelScale;
        out->accelY = (float)rawAccelY * accelScale;
        out->accelZ = (float)rawAccelZ * accelScale;
    }
}
