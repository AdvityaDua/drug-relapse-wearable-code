#pragma once

#include <stdint.h>
#include <stdbool.h>

namespace MAX30205
{
    /**
     * @brief Initializes the MAX30205 sensor in Shutdown mode to save battery.
     * @return true if successful
     */
    bool init();

    /**
     * @brief Triggers a single one-shot conversion, waits for completion,
     * and reads the highly accurate body temperature.
     * 
     * @return Temperature in degrees Celsius.
     */
    float readTemperature();
}
