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
  
  const char* SYNC_FILE_PATH = "/littlefs/sync.log";
  
  // Rename the current data.log to sync.log to avoid race conditions
  if (rename(FILE_PATH, SYNC_FILE_PATH) != 0) {
      ESP_LOGW(TAG, "Failed to rename log file, or no data exists.");
      return;
  }

  FILE *f = fopen(SYNC_FILE_PATH, "rb");
  if (f == NULL) {
    ESP_LOGW(TAG, "Failed to open sync log file.");
    // In case of failure, try to restore the original name (optional)
    rename(SYNC_FILE_PATH, FILE_PATH);
    return;
  }

  char buffer[240]; // Safe chunk size under BLE MTU of 244
  size_t bytesRead;
  while ((bytesRead = fread(buffer, 1, sizeof(buffer), f)) > 0) {
    ble.notifyData((const uint8_t *)buffer, bytesRead);

    // Allow NimBLE stack time to process the notification
    vTaskDelay(pdMS_TO_TICKS(15));
  }

  fclose(f);
  ESP_LOGI(TAG, "BLE data sync complete.");

  // Delete the sync file after successful streaming
  if (remove(SYNC_FILE_PATH) == 0) {
      ESP_LOGI(TAG, "Sync file deleted.");
  } else {
      ESP_LOGE(TAG, "Failed to delete sync file.");
  }
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
