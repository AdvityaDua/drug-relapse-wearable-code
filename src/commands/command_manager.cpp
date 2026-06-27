#include "command_manager.h"

#include "ble/ble_manager.h"
#include "config.h"
#include "power/power_manager.h"
#include "sensors/sensor_manager.h"
#include "../storage/storage_manager.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstring>
#include <sys/time.h>

static const char *TAG = "CMD";

CommandManager::CommandManager() {
  collecting = false;
  sampleIntervalMs = DEFAULT_SAMPLE_INTERVAL_MS;
}

void CommandManager::begin() {
  ESP_LOGI(TAG, "Manager Ready");
  ESP_LOGI(TAG, "Sample interval (ms): %lu", (unsigned long)sampleIntervalMs);
}

void CommandManager::processPending(BLEManager &ble, PowerManager &power) {
  if (!ble.hasNewCommand()) {
    return;
  }

  const CommandPacket packet = ble.getCommand();
  const uint8_t rawCommand = static_cast<uint8_t>(packet.command);
  const StatusCode status = executeCommand(packet, power, ble);

  logCommandResult(rawCommand, status);
}

bool CommandManager::isCollecting() const { return collecting; }

uint32_t CommandManager::getSampleIntervalMs() const {
  return sampleIntervalMs;
}

StatusCode CommandManager::executeCommand(const CommandPacket &packet,
                                          PowerManager &power,
                                          BLEManager &ble) {
  uint8_t rawCommand = static_cast<uint8_t>(packet.command);

  if (!CommandUtils::isKnownCommand(rawCommand)) {
    return StatusCode::INVALID_COMMAND;
  }

  switch (packet.command) {
  case Command::START_COLLECTION:
    if (collecting) {
      return StatusCode::BUSY;
    }
    collecting = true;
    ESP_LOGI(TAG, "Data collection started.");
    return StatusCode::SUCCESS;

  case Command::STOP_COLLECTION:
    if (!collecting) {
      return StatusCode::SUCCESS;
    }
    collecting = false;
    ESP_LOGI(TAG, "Data collection stopped.");
    return StatusCode::SUCCESS;

  case Command::SYNC_DATA:
    ESP_LOGI(TAG, "SYNC_DATA requested - Streaming to BLE");
    StorageManager::streamDataToBLE(ble);
    return StatusCode::SUCCESS;

  case Command::GET_STATUS:
    ESP_LOGI(TAG, "Status -> collecting: %s", collecting ? "true" : "false");
    return StatusCode::SUCCESS;

  case Command::GET_BATTERY: {
    ESP_LOGI(TAG, "GET_BATTERY requested - Reading ADC and sending to BLE");
    uint8_t battPct = power.getBatteryPercentage();
    ble.notifyBattery(battPct);
    return StatusCode::SUCCESS;
  }

  case Command::GET_LIVE_DATA: {
    ESP_LOGI(TAG, "GET_LIVE_DATA requested - Taking instant reading");
    char jsonBuffer[1024];
    if (SensorManager::takeReading(jsonBuffer, sizeof(jsonBuffer))) {
      size_t len = strlen(jsonBuffer);
      if (len > 0) {
        ble.notifyData((const uint8_t*)jsonBuffer, len);
      }
    }
    logCommandResult(rawCommand, StatusCode::SUCCESS);
    return StatusCode::SUCCESS;
  }

  case Command::SET_TIME: {
    if (packet.length >= 4) {
        uint32_t epoch;
        memcpy(&epoch, packet.payload, sizeof(epoch));
        
        struct timeval tv;
        tv.tv_sec = epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
        
        ESP_LOGI(TAG, "System time updated to epoch: %lu", (unsigned long)epoch);
        return StatusCode::SUCCESS;
    }
    return StatusCode::ERROR;
  }

  case Command::RESTART_DEVICE:
    logCommandResult(rawCommand, StatusCode::SUCCESS);
    ESP_LOGI(TAG, "Restart requested");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_restart();
    return StatusCode::SUCCESS;

  case Command::SET_SAMPLE_INTERVAL:
    if (packet.length >= 4) {
      uint32_t interval;
      memcpy(&interval, packet.payload, sizeof(interval));
      if (interval >= MIN_SAMPLE_INTERVAL_MS &&
          interval <= MAX_SAMPLE_INTERVAL_MS) {
        sampleIntervalMs = interval;
      } else {
        return StatusCode::ERROR;
      }
    } else {
      sampleIntervalMs = DEFAULT_SAMPLE_INTERVAL_MS;
    }
    ESP_LOGI(TAG, "Sample interval set (ms): %lu",
             (unsigned long)sampleIntervalMs);
    return StatusCode::SUCCESS;

  default:
    return StatusCode::INVALID_COMMAND;
  }
}

void CommandManager::logCommandResult(uint8_t rawCommand,
                                      StatusCode status) const {
  const char *statusStr = "UNKNOWN_STATUS";

  switch (status) {
  case StatusCode::SUCCESS:
    statusStr = "SUCCESS";
    break;
  case StatusCode::ERROR:
    statusStr = "ERROR";
    break;
  case StatusCode::BUSY:
    statusStr = "BUSY";
    break;
  case StatusCode::INVALID_COMMAND:
    statusStr = "INVALID_COMMAND";
    break;
  case StatusCode::BLE_DISCONNECTED:
    statusStr = "BLE_DISCONNECTED";
    break;
  case StatusCode::NOT_IMPLEMENTED:
    statusStr = "NOT_IMPLEMENTED";
    break;
  default:
    break;
  }

  ESP_LOGI(TAG, "%s (0x%02X) -> %s", CommandUtils::commandToString(rawCommand),
           rawCommand, statusStr);
}
