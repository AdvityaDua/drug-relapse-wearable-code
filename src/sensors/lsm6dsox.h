#pragma once

#include <stdint.h>
#include <stdbool.h>

namespace LSM6DSOX
{
    struct LSM6DSOX_Data {
        float temp;
        
        // Gyroscope in degrees per second (dps)
        float gyroX, gyroY, gyroZ;
        
        // Accelerometer in m/s^2
        float accelX, accelY, accelZ;
    };

    /**
     * @brief Initializes the LSM6DSOX sensor (Accel & Gyro)
     * @return true if successful
     */
    bool init();

    /**
     * @brief Reads all LSM6DSOX sensor data
     */
    void readAll(LSM6DSOX_Data* out);
}
