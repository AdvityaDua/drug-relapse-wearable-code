#include "storage_manager.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "StorageManager";
static const char *BASE_PATH = "/littlefs";
static const char *FILE_PATH = "/littlefs/data.log";

namespace StorageManager {
bool init() {
  ESP_LOGI(TAG, "Initializing LittleFS");

  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = BASE_PATH;
  conf.partition_label = "storage";
  conf.format_if_mount_failed = true;
  conf.dont_mount = false;

  // Use settings defined above to initialize and mount LittleFS filesystem.
  // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_littlefs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find LittleFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
    }
    return false;
  }

  size_t total = 0, used = 0;
  ret = esp_littlefs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)",
             esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }

  return true;
}

bool logSensorData(const char *jsonData) {
  if (jsonData == NULL)
    return false;

  ESP_LOGI(TAG, "Opening file %s for appending", FILE_PATH);
  FILE *f = fopen(FILE_PATH, "a");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for appending");
    return false;
  }

  fprintf(f, "%s\n", jsonData);
  fclose(f);

  ESP_LOGI(TAG, "Successfully appended JSON data to log.");
  return true;
}

void clearData() {
  ESP_LOGI(TAG, "Clearing log file: %s", FILE_PATH);
  FILE *f = fopen(FILE_PATH, "w");
  if (f != NULL) {
    fclose(f);
    ESP_LOGI(TAG, "Log file cleared successfully.");
  } else {
    ESP_LOGE(TAG, "Failed to clear log file.");
  }
}
void streamDataToBLE(BLEManager& ble) {
  if (!ble.isConnected()) {
    ESP_LOGW(TAG, "Cannot stream data, BLE is disconnected.");
    return;
  }

  ESP_LOGI(TAG, "Starting BLE data sync...");
  FILE *f = fopen(FILE_PATH, "r");
  if (f == NULL) {
    ESP_LOGW(TAG, "No data file to sync.");
    return;
  }

  char buffer[512]; // Large enough for one full JSON payload line
  while (fgets(buffer, sizeof(buffer), f) != NULL) {
    size_t len = strlen(buffer);
    if (len > 0) {
      ble.notifyData((const uint8_t *)buffer, len);

      // Allow NimBLE stack time to process the notification
      vTaskDelay(pdMS_TO_TICKS(15));
    }
  }

  fclose(f);
  ESP_LOGI(TAG, "BLE data sync complete.");

  // Finally, delete the file so we start fresh for the next interval
  clearData();
}

bool saveCalibrationProfile(const uint8_t* data, size_t length)
{
    FILE* f = fopen("/littlefs/bno_calib.bin", "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open bno_calib.bin for writing");
        return false;
    }
    size_t written = fwrite(data, 1, length, f);
    fclose(f);
    return written == length;
}

bool loadCalibrationProfile(uint8_t* data, size_t length)
{
    FILE* f = fopen("/littlefs/bno_calib.bin", "rb");
    if (f == NULL) {
        // It's normal for the file to not exist on first boot
        return false;
    }
    size_t readLen = fread(data, 1, length, f);
    fclose(f);
    return readLen == length;
}

} // namespace StorageManager
