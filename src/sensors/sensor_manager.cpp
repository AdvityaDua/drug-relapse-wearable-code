#include "sensor_manager.h"
#include "max30102.h"
#include "bno055.h"
#include "tla2022.h"
#include "max30205.h"
#include "config.h"
#include "../storage/storage_manager.h"
#include <sys/time.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

static const char* TAG = "SensorManager";

namespace SensorManager
{
    void powerOn()
    {
        ESP_LOGI(TAG, "Powering ON sensors via GPIO...");
        gpio_set_direction((gpio_num_t)PIN_SENSOR_POWER, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)PIN_SENSOR_POWER, 1);
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait 50ms for sensors to power up stability
        
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

        // Initialize sensors
        if (!MAX30102::init()) {
            ESP_LOGE(TAG, "MAX30102 initialization failed!");
        } else {
            ESP_LOGI(TAG, "MAX30102 initialized successfully.");
        }

        if (!BNO055::init()) {
            ESP_LOGE(TAG, "BNO055 initialization failed!");
        } else {
            ESP_LOGI(TAG, "BNO055 initialized successfully.");
            // Auto-load calibration
            uint8_t calibData[22];
            if (StorageManager::loadCalibrationProfile(calibData, sizeof(calibData))) {
                ESP_LOGI(TAG, "Loaded BNO055 calibration from LittleFS, injecting...");
                BNO055::setCalibrationProfile(calibData);
            }
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

        // 1. Read MAX30102 (takes ~1 second)
        MAX30102::MAX30102_Data maxData = MAX30102::readAndCalculate();
        vTaskDelay(pdMS_TO_TICKS(100)); // Simulate time taken

        // 2. Read BNO055 (instantaneous I2C burst)
        BNO055::BNO055_Data bnoData;
        BNO055::readAll(&bnoData);

        // Auto-save calibration if fully calibrated
        if (bnoData.calibSys == 3 && bnoData.calibGyro == 3 && bnoData.calibAccel == 3 && bnoData.calibMag == 3) {
            uint8_t currentCalib[22];
            if (BNO055::getCalibrationProfile(currentCalib)) {
                // Since this runs every minute, we could check if it changed before writing to save flash wear,
                // but for now we just overwrite to ensure it's saved.
                StorageManager::saveCalibrationProfile(currentCalib, sizeof(currentCalib));
                ESP_LOGI(TAG, "BNO055 fully calibrated - profile saved to flash.");
            }
        }

        // 3. Read TinyGSR (takes ~72ms for 8 samples)
        int16_t gsrValue = TLA2022::readGSR();

        // 4. Read MAX30205 (takes ~55ms for one-shot)
        float bodyTemp = MAX30205::readTemperature();

        // Get Timestamp
        struct timeval tv;
        gettimeofday(&tv, NULL);

        // 5. Construct JSON
        if (outBuffer != nullptr && maxLen > 0) {
            snprintf(outBuffer, maxLen,
                "{\n"
                "  \"time\": %ld,\n"
                "  \"gsr\": %d,\n"
                "  \"bodyTemp\": %.2f,\n"
                "  \"max30102\": {\n"
                "    \"hr\": %ld,\n"
                "    \"validHR\": %d,\n"
                "    \"spo2\": %ld,\n"
                "    \"validSPO2\": %d\n"
                "  },\n"
                "  \"bno055\": {\n"
                "    \"euler\": [ %.2f, %.2f, %.2f ],\n"
                "    \"quat\": [ %.2f, %.2f, %.2f, %.2f ],\n"
                "    \"linear\": [ %.2f, %.2f, %.2f ],\n"
                "    \"gravity\": [ %.2f, %.2f, %.2f ],\n"
                "    \"accel\": [ %.2f, %.2f, %.2f ],\n"
                "    \"gyro\": [ %.2f, %.2f, %.2f ],\n"
                "    \"mag\": [ %.2f, %.2f, %.2f ],\n"
                "    \"temp\": %d,\n"
                "    \"calib\": [ %d, %d, %d, %d ]\n"
                "  }\n"
                "}",
                (long)tv.tv_sec,
                gsrValue, bodyTemp,
                maxData.heartRate, maxData.validHR, maxData.spo2, maxData.validSPO2,
                bnoData.eulerHeading, bnoData.eulerRoll, bnoData.eulerPitch,
                bnoData.quatW, bnoData.quatX, bnoData.quatY, bnoData.quatZ,
                bnoData.linearAccelX, bnoData.linearAccelY, bnoData.linearAccelZ,
                bnoData.gravityX, bnoData.gravityY, bnoData.gravityZ,
                bnoData.accelX, bnoData.accelY, bnoData.accelZ,
                bnoData.gyroX, bnoData.gyroY, bnoData.gyroZ,
                bnoData.magX, bnoData.magY, bnoData.magZ,
                bnoData.temp,
                bnoData.calibSys, bnoData.calibGyro, bnoData.calibAccel, bnoData.calibMag
            );

            ESP_LOGI(TAG, "Generated JSON Data:\n%s", outBuffer);
        }

        powerOff();
        xSemaphoreGive(sensorMutex);
        
        return true;
    }
}
