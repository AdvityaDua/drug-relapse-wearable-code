#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#if USE_WIFI
#include "wifi/wifi_manager.h"
#else
#include "ble/ble_manager.h"
#endif
#include "commands/command_manager.h"
#include "power/power_manager.h"
#include "sensors/sensor_manager.h"
#include "storage/storage_manager.h"

static const char *TAG = "MAIN";

PowerManager power;
TransportManager transport;
CommandManager commandManager;

// Background task that runs every X milliseconds
void dataCollectionTask(void *pvParameters) {
  while (true) {
    if (commandManager.isCollecting()) {
      ESP_LOGI(TAG, "Executing scheduled autonomous collection cycle...");
      char jsonBuffer[512];
      if (SensorManager::takeReading(jsonBuffer, sizeof(jsonBuffer))) {
        if (!StorageManager::logSensorData(jsonBuffer)) {
          ESP_LOGE(TAG, "Failed to write sensor data to flash.");
        } else {
          ESP_LOGI(TAG,
                   "Successfully written 1-second sensor reading to flash.");
        }
      }
    }

    // Delay for the configured sample interval (e.g., 1 second).
    // During this delay, the ESP32 will automatically enter Light Sleep to save
    // battery!
    vTaskDelay(pdMS_TO_TICKS(commandManager.getSampleIntervalMs()));
  }
}

#include "nvs_flash.h"

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "==================================");
  ESP_LOGI(TAG, " Health Wearable Firmware");
  ESP_LOGI(TAG, "==================================");

  // Initialize NVS (Required for WiFi)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  power.begin();

  switch (power.getWakeupCause()) {
  case ESP_SLEEP_WAKEUP_GPIO: {
    ESP_LOGI(TAG, "[BOOT] Wakeup from Button — verifying long press...");

    // The hardware wakes on any LOW edge. We need to confirm the
    // user is holding the button for the full LONG_PRESS_DURATION_MS
    // before actually booting. If they let go early, go back to sleep.
    uint32_t held = 0;
    while (gpio_get_level(PIN_BUTTON) == 0 && held < LONG_PRESS_DURATION_MS) {
      vTaskDelay(pdMS_TO_TICKS(50));
      held += 50;
    }

    if (held < LONG_PRESS_DURATION_MS) {
      ESP_LOGI(TAG,
               "[BOOT] Button released too early (%lums) — back to sleep",
               (unsigned long)held);
      power.enterDeepSleep(); // goes back to deep sleep immediately
    }

    ESP_LOGI(TAG, "[BOOT] Long press confirmed — booting up");
    break;
  }

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

  ESP_LOGI(TAG, "[POWER] Device Active");

  // Power-on LED pattern: 3 quick flashes then solid ON
  gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_LED, 1); // LED OFF (active-low)
  for (int i = 0; i < 3; i++) {
    gpio_set_level(PIN_LED, 0); // LED ON
    vTaskDelay(pdMS_TO_TICKS(80));
    gpio_set_level(PIN_LED, 1); // LED OFF
    vTaskDelay(pdMS_TO_TICKS(80));
  }
  gpio_set_level(PIN_LED, 0); // LED solid ON — stays on while device is active

  transport.begin();
  commandManager.begin();

  // Initialize Storage (LittleFS)
  if (!StorageManager::init()) {
    ESP_LOGE(TAG, "Failed to initialize Storage Manager!");
  } else {
    ESP_LOGI(TAG, "Storage Manager initialized successfully.");
  }

  // Start the button monitor (polls D2 for long-press to trigger deep sleep)
  power.startButtonMonitor();

  ESP_LOGI(TAG, "[SYSTEM] Ready");

  // Spawn the background data collection task
  xTaskCreate(dataCollectionTask, "DataCollection", 4096, NULL, 5, NULL);

  uint32_t lastBatteryUpdate = 0;

  /* Main loop — blocks on semaphore, allows light sleep between commands */
  while (true) {
    transport.waitForCommand(pdMS_TO_TICKS(1000));
    commandManager.processPending(transport, power);

    if (transport.isConnected()) {
      uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
      if (now - lastBatteryUpdate >= 5000) { // Update battery every 5 seconds
        uint8_t batteryPct = power.getBatteryPercentage();
        transport.notifyBattery(batteryPct);
        lastBatteryUpdate = now;
      }
    }

    // Check if the button monitor detected a long-press
    if (power.isShutdownRequested()) {
      ESP_LOGI(TAG, "[POWER] Shutdown requested via button long-press");
      power.enterDeepSleep();
    }
  }
}