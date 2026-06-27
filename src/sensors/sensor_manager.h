#pragma once

#include <stdint.h>

namespace SensorManager
{
    /**
     * @brief Powers up sensors and initializes the I2C bus.
     */
    void powerOn();

    /**
     * @brief Powers down sensors and disables the I2C bus to save battery.
     */
    void powerOff();

    /**
     * @brief Takes a reading of all sensors, constructs a JSON string, and places it in outBuffer.
     * @param outBuffer Buffer to hold the JSON string.
     * @param maxLen Maximum length of the buffer.
     * @return true if successful, false if busy or error.
     */
    bool takeReading(char* outBuffer, size_t maxLen);
}
