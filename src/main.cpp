#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "power/power_manager.h"
#include "ble/ble_manager.h"
#include "commands/command_manager.h"
#include "sensors/sensor_manager.h"
#include "storage/storage_manager.h"

static const char* TAG = "MAIN";

PowerManager power;
BLEManager ble;
CommandManager commandManager;

// Background task that runs every X milliseconds
void dataCollectionTask(void *pvParameters)
{
    while (true)
    {
        if (commandManager.isCollecting())
        {
            ESP_LOGI(TAG, "Executing scheduled autonomous collection cycle...");
            char jsonBuffer[512];
            if (SensorManager::takeReading(jsonBuffer, sizeof(jsonBuffer))) {
                if (!StorageManager::logSensorData(jsonBuffer)) {
                    ESP_LOGE(TAG, "Failed to write sensor data to flash.");
                } else {
                    ESP_LOGI(TAG, "Successfully written 1-minute sensor reading to flash.");
                }
            }
        }

        // Delay for the configured sample interval (e.g., 1 minute).
        // During this delay, the ESP32 will automatically enter Light Sleep to save battery!
        vTaskDelay(pdMS_TO_TICKS(commandManager.getSampleIntervalMs()));
    }
}

#include "nvs_flash.h"

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "==================================");
    ESP_LOGI(TAG, " Health Wearable Firmware");
    ESP_LOGI(TAG, "==================================");

    // Initialize NVS (Required for BLE Bonding/Security)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    power.begin();

    switch (power.getWakeupCause())
    {
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGI(TAG, "[BOOT] Wakeup from Jumper (Bus Low Enable)");
            break;

        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "[BOOT] Wakeup from Timer");
            break;

        case ESP_SLEEP_WAKEUP_UNDEFINED:
            ESP_LOGI(TAG, "[BOOT] Cold Boot");
            break;

        default:
            ESP_LOGI(TAG, "[BOOT] Other Wakeup");
            break;
    }

    if (power.shouldEnterDeepSleep())
    {
        ESP_LOGI(TAG, "[POWER] Bus Low Disabled");
        ESP_LOGI(TAG, "[POWER] Going to Deep Sleep");

        vTaskDelay(pdMS_TO_TICKS(200));

        power.enterDeepSleep();
    }

    ESP_LOGI(TAG, "[POWER] Device Enabled");

    ble.begin();
    commandManager.begin();

    // Initialize Storage (LittleFS)
    if (!StorageManager::init()) {
        ESP_LOGE(TAG, "Failed to initialize Storage Manager!");
    } else {
        ESP_LOGI(TAG, "Storage Manager initialized successfully.");
    }

    ESP_LOGI(TAG, "[SYSTEM] Ready");

    // Spawn the background data collection task
    xTaskCreate(dataCollectionTask, "DataCollection", 4096, NULL, 5, NULL);

    /* Main loop — blocks on semaphore, allows light sleep between commands */
    while (true)
    {
        ble.waitForCommand(pdMS_TO_TICKS(1000));
        commandManager.processPending(ble, power);
    }
}