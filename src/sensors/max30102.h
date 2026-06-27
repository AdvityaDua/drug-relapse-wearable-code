#pragma once
#include <stdint.h>
#include <stdbool.h>

namespace MAX30102
{
    struct MAX30102_Data {
        int32_t heartRate;
        int8_t validHR;
        int32_t spo2;
        int8_t validSPO2;
    };

    /**
     * @brief Resets and configures the MAX30102 sensor
     * @return true if successful
     */
    bool init();

    void powerOff();

    /**
     * @brief Collects 100 samples from the FIFO, calculates HR and SpO2 using Maxim algorithm, and prints it
     */
    MAX30102_Data readAndCalculate();
}
