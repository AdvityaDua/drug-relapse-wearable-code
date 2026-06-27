#pragma once

#include <stdint.h>
#include <stdbool.h>

namespace TLA2022
{
    /**
     * @brief Initializes the TLA2022 sensor in single-shot mode.
     * @return true if successful
     */
    bool init();

    /**
     * @brief Measures the Galvanic Skin Response.
     * Starts 8 single-shot ADC conversions, waits for them, and returns the sum,
     * replicating the Protocentral 8-tap FIR filter.
     * 
     * @return The filtered GSR ADC value.
     */
    int16_t readGSR();
}
