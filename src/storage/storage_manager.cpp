#include "storage_manager.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#if USE_WIFI
#include "wifi/wifi_manager.h"
#else
#include "ble/ble_manager.h"
#endif

static const char *TAG = "StorageManager";
static const char *BASE_PATH = "/littlefs";
static const char *FILE_PATH = "/littlefs/data.csv";
static const char *CSV_HEADER = "Timestamp,GSR,BodyTemp,HeartRate,HRValid,SpO2,SpO2Valid,EulerHeading,EulerRoll,EulerPitch,QuatW,QuatX,QuatY,QuatZ,LinearAccelX,LinearAccelY,LinearAccelZ,GravityX,GravityY,GravityZ,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,MagX,MagY,MagZ,BNOTemp,CalibSys,CalibGyro,CalibAccel,CalibMag";

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

bool logSensorData(const char *csvData) {
  if (csvData == NULL)
    return false;

  struct stat st;
  if (stat(FILE_PATH, &st) == 0) {
      if (st.st_size > 1000 * 1024) { // 1 MB limit
          ESP_LOGE(TAG, "data.csv exceeds 1MB limit. Deleting to prevent filesystem corruption.");
          remove(FILE_PATH);
      }
  }

  bool fileExists = true;
  FILE *check = fopen(FILE_PATH, "r");
  if (check == NULL) {
      fileExists = false;
  } else {
      fclose(check);
  }

  ESP_LOGI(TAG, "Opening file %s for appending", FILE_PATH);
  FILE *f = fopen(FILE_PATH, "a");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for appending");
    return false;
  }

  if (!fileExists) {
      fprintf(f, "%s\n", CSV_HEADER);
  }

  fprintf(f, "%s\n", csvData);
  fclose(f);

  ESP_LOGI(TAG, "Successfully appended CSV data to file.");
  return true;
}

void clearData() {
  ESP_LOGI(TAG, "Clearing log file: %s", FILE_PATH);
  FILE *f = fopen(FILE_PATH, "w");
  if (f != NULL) {
    fprintf(f, "%s\n", CSV_HEADER);
    fclose(f);
    ESP_LOGI(TAG, "Log file cleared and initialized with headers.");
  } else {
    ESP_LOGE(TAG, "Failed to clear log file.");
  }
}
bool streamData(TransportManager& transport) {
  if (!transport.isConnected()) {
    ESP_LOGW(TAG, "Cannot stream data, client is disconnected.");
    return false;
  }

  ESP_LOGI(TAG, "Starting data sync...");
  
  const char* SYNC_FILE_PATH = "/littlefs/sync.csv";
  
  // Check if a previous sync file exists
  FILE *check = fopen(SYNC_FILE_PATH, "r");
  if (check == NULL) {
      // No previous sync.csv exists, so we rename data.csv to sync.csv
      if (rename(FILE_PATH, SYNC_FILE_PATH) != 0) {
          ESP_LOGW(TAG, "Failed to rename log file, or no data exists.");
          return false;
      }
  } else {
      fclose(check);
      ESP_LOGI(TAG, "Resuming previous sync from sync.csv...");
  }

  FILE *f = fopen(SYNC_FILE_PATH, "rb");
  if (f == NULL) {
    ESP_LOGW(TAG, "Failed to open sync log file.");
    return false;
  }

  // Rewind file pointer to the beginning
  fseek(f, 0, SEEK_SET);

  char buffer[150]; // Smaller 150 byte chunk size
  size_t bytesRead;
  size_t totalBytes = 0;
  int chunkCount = 0;
  bool success = true;
  
  while ((bytesRead = fread(buffer, 1, sizeof(buffer), f)) > 0) {
    if (!transport.notifyData((const uint8_t *)buffer, bytesRead)) {
        ESP_LOGE(TAG, "Failed to send data chunk. Client disconnected?");
        success = false;
        break;
    }
    totalBytes += bytesRead;
    chunkCount++;

    // Allow TCP stack time to process the notification
    vTaskDelay(pdMS_TO_TICKS(15));
  }

  fclose(f);
  
  if (!success) {
      return false; // Exit early without deleting sync.csv
  }

  // Send END_SYNC sentinel so the app knows streaming is complete
  const char* sentinel = "END_SYNC\n";
  vTaskDelay(pdMS_TO_TICKS(15));
  if (!transport.notifyData((const uint8_t *)sentinel, strlen(sentinel))) {
      return false; // Sentinel failed to send, assume disconnected
  }
  
  ESP_LOGI(TAG, "Data sync complete. Sent %d chunks, %d bytes total.", chunkCount, totalBytes);

  // Delete the sync file only after successful streaming
  if (remove(SYNC_FILE_PATH) == 0) {
      ESP_LOGI(TAG, "Sync file deleted.");
  } else {
      ESP_LOGE(TAG, "Failed to delete sync file.");
  }
  
  return true;
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
