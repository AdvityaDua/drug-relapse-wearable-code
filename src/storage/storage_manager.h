#pragma once

#include <stdbool.h>
#include "../commands/command_manager.h"

namespace StorageManager
{
    /**
     * @brief Mounts the LittleFS partition named 'storage'. 
     * If the partition is corrupt or unformatted, it will format it automatically.
     * @return true on success, false on failure.
     */
    bool init();

    /**
     * @brief Appends the JSON string to the internal data.log file, separated by a newline.
     * @param jsonData The null-terminated JSON string to append.
     * @return true on success, false if file could not be written.
     */
    bool logSensorData(const char* jsonData);

    /**
     * @brief Clears the internal data.log file completely (called after a successful BLE sync).
     */
    void clearData();

    bool streamData(TransportManager& transport);

    /**
     * @brief Saves a binary calibration profile to LittleFS
     */
    bool saveCalibrationProfile(const uint8_t* data, size_t length);

    /**
     * @brief Loads a binary calibration profile from LittleFS
     */
    bool loadCalibrationProfile(uint8_t* data, size_t length);
}
