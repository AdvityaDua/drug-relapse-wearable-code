#include "sensor_manager.h"
#include "max30102.h"
#include "lsm6dsox.h"
#include "tla2022.h"
#include "max30205.h"
#include "config.h"
#include "../storage/storage_manager.h"
#include <sys/time.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include <cstring>

static const char* TAG = "SensorManager";

namespace SensorManager
{
    void powerOn()
    {
        static bool isPoweredOn = false;
        if (isPoweredOn) return;
        isPoweredOn = true;

        ESP_LOGI(TAG, "Powering ON sensors via GPIO...");
        gpio_set_direction((gpio_num_t)PIN_SENSOR_POWER, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)PIN_SENSOR_POWER, 1);
        vTaskDelay(pdMS_TO_TICKS(650)); // Wait 650ms for BNO055 and other sensors to fully boot up
        
        ESP_LOGI(TAG, "Initializing I2C Master...");
        
        i2c_port_t i2c_master_port = (i2c_port_t)I2C_MASTER_NUM;
        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
        conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
        
        esp_err_t err = i2c_param_config(i2c_master_port, &conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2C param config failed");
            return;
        }

        err = i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2C driver install failed");
            return;
        }

        ESP_LOGI(TAG, "I2C Master initialized on SDA=%d, SCL=%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

        // --- I2C Bus Scan (debug) ---
        ESP_LOGI(TAG, "Scanning I2C bus for connected devices...");
        int devicesFound = 0;
        for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
            // Send a zero-length write to probe the address
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            esp_err_t probe = i2c_master_cmd_begin((i2c_port_t)I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
            i2c_cmd_link_delete(cmd);

            if (probe == ESP_OK) {
                ESP_LOGI(TAG, "  -> Device found at 0x%02X", addr);
                devicesFound++;
            }
        }
        ESP_LOGI(TAG, "I2C scan complete: %d device(s) found.", devicesFound);

        // Initialize sensors
        if (!MAX30102::init()) {
            ESP_LOGE(TAG, "MAX30102 initialization failed!");
        } else {
            ESP_LOGI(TAG, "MAX30102 initialized successfully.");
        }

        if (!LSM6DSOX::init()) {
            ESP_LOGE(TAG, "LSM6DSOX initialization failed!");
        } else {
            ESP_LOGI(TAG, "LSM6DSOX initialized successfully.");
        }

        if (!TLA2022::init()) {
            ESP_LOGE(TAG, "TinyGSR (TLA2022) initialization failed!");
        } else {
            ESP_LOGI(TAG, "TinyGSR (TLA2022) initialized successfully.");
        }

        if (!MAX30205::init()) {
            ESP_LOGE(TAG, "MAX30205 body temperature initialization failed!");
        } else {
            ESP_LOGI(TAG, "MAX30205 body temperature initialized successfully.");
        }
    }

    void powerOff()
    {
        ESP_LOGI(TAG, "Powering OFF sensors to save battery...");
        
        // Deinitialize I2C driver so it doesn't hold the pins high while power is cut
        i2c_driver_delete((i2c_port_t)I2C_MASTER_NUM);

        // Reset I2C pins so they don't back-power the sensors via internal pullups
        gpio_reset_pin((gpio_num_t)I2C_MASTER_SDA_IO);
        gpio_reset_pin((gpio_num_t)I2C_MASTER_SCL_IO);

        // Cut GPIO power
        gpio_set_level((gpio_num_t)PIN_SENSOR_POWER, 0);
    }

    bool takeReading(char* outBuffer, size_t maxLen)
    {
        static SemaphoreHandle_t sensorMutex = NULL;

        if (sensorMutex == NULL) {
            sensorMutex = xSemaphoreCreateMutex();
        }

        // Wait up to 2 seconds for the mutex (in case another thread is reading)
        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ESP_LOGW(TAG, "SensorManager is busy, dropping takeReading() request.");
            return false;
        }

        ESP_LOGI(TAG, "--- SensorManager::takeReading() Started ---");
        powerOn();

        // ---------------------------------------------------------
        // DIAGNOSTIC: Full I2C Bus Scan
        // ---------------------------------------------------------
        ESP_LOGI(TAG, "┌── I2C FULL BUS SCAN ─────────────────────");
        int foundCount = 0;
        for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            esp_err_t probe = i2c_master_cmd_begin((i2c_port_t)I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
            i2c_cmd_link_delete(cmd);

            if (probe == ESP_OK) {
                foundCount++;
                const char* name = "Unknown Device";
                if (addr == 0x6A) name = "LSM6DSOX (Low)";
                else if (addr == 0x6B) name = "LSM6DSOX (High)";
                else if (addr == 0x48) name = "MAX30205 (Temp)";
                else if (addr == 0x49) name = "TLA2022 (GSR)";
                else if (addr == 0x57) name = "MAX30102 (PPG)";
                
                ESP_LOGI(TAG, "│ [DETECTED] 0x%02X -> %s", addr, name);
            }
        }
        if (foundCount == 0) {
            ESP_LOGW(TAG, "│ NO I2C DEVICES DETECTED!");
        }
        ESP_LOGI(TAG, "└──────────────────────────────────────────");
        // ---------------------------------------------------------

        // 1. Read MAX30102 (takes ~1 second)
        MAX30102::MAX30102_Data maxData = MAX30102::readAndCalculate();
        // vTaskDelay(pdMS_TO_TICKS(100)); // Simulate time taken

        ESP_LOGI(TAG, "┌── MAX30102 (Pulse Oximeter) ──────────────");
        ESP_LOGI(TAG, "│ Heart Rate : %ld bpm (valid: %s)", maxData.heartRate, maxData.validHR ? "YES" : "NO");
        ESP_LOGI(TAG, "│ SpO2       : %ld %% (valid: %s)", maxData.spo2, maxData.validSPO2 ? "YES" : "NO");
        ESP_LOGI(TAG, "└──────────────────────────────────────────");

        // 2. Read LSM6DSOX (instantaneous I2C burst)
        LSM6DSOX::LSM6DSOX_Data lsmData;
        LSM6DSOX::readAll(&lsmData);

        ESP_LOGI(TAG, "┌── LSM6DSOX (6-DOF IMU) ──────────────────");
        ESP_LOGI(TAG, "│ Accel      : X=%.2f Y=%.2f Z=%.2f m/s²", lsmData.accelX, lsmData.accelY, lsmData.accelZ);
        ESP_LOGI(TAG, "│ Gyro       : X=%.2f Y=%.2f Z=%.2f dps", lsmData.gyroX, lsmData.gyroY, lsmData.gyroZ);
        ESP_LOGI(TAG, "│ Temp       : %.2f °C", lsmData.temp);
        ESP_LOGI(TAG, "└──────────────────────────────────────────");

        // 3. Read TinyGSR (takes ~72ms for 8 samples)
        int16_t gsrValue = TLA2022::readGSR();

        ESP_LOGI(TAG, "┌── TLA2022 (GSR Sensor) ──────────────────");
        ESP_LOGI(TAG, "│ GSR Value  : %d (8-tap filtered sum)", gsrValue);
        ESP_LOGI(TAG, "└──────────────────────────────────────────");

        // 4. Read MAX30205 (takes ~55ms for one-shot)
        float bodyTemp = MAX30205::readTemperature();

        ESP_LOGI(TAG, "┌── MAX30205 (Body Temperature) ───────────");
        ESP_LOGI(TAG, "│ Body Temp  : %.2f °C", bodyTemp);
        ESP_LOGI(TAG, "└──────────────────────────────────────────");

        // Get Timestamp
        struct timeval tv;
        gettimeofday(&tv, NULL);

        // 5. Construct CSV
        if (outBuffer != nullptr && maxLen > 0) {
            snprintf(outBuffer, maxLen,
                "%ld,%d,%.2f,%ld,%d,%ld,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
                (long)tv.tv_sec,
                gsrValue, bodyTemp,
                maxData.heartRate, maxData.validHR, maxData.spo2, maxData.validSPO2,
                lsmData.accelX, lsmData.accelY, lsmData.accelZ,
                lsmData.gyroX, lsmData.gyroY, lsmData.gyroZ,
                lsmData.temp
            );

            ESP_LOGI(TAG, "Generated CSV Data:\n%s", outBuffer);
        }

        // powerOff(); // User requested to keep sensors powered on
        xSemaphoreGive(sensorMutex);
        
        return true;
    }
}
