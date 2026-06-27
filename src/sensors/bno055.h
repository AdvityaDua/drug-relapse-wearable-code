#pragma once

#include <stdint.h>
#include <stdbool.h>

namespace BNO055
{
    struct BNO055_Data {
        // Absolute Orientation
        float eulerHeading, eulerRoll, eulerPitch;
        float quatW, quatX, quatY, quatZ;
        
        // Vectors
        float linearAccelX, linearAccelY, linearAccelZ;
        float gravityX, gravityY, gravityZ;
        float accelX, accelY, accelZ;
        float gyroX, gyroY, gyroZ;
        float magX, magY, magZ;

        int8_t temp;
        uint8_t calibSys, calibGyro, calibAccel, calibMag;
    };

    /**
     * @brief Initializes the BNO055 sensor in NDOF fusion mode
     * @return true if successful
     */
    bool init();

    /**
     * @brief Reads all BNO055 sensor data
     */
    void readAll(BNO055_Data* out);

    /**
     * @brief Reads the 22-byte calibration profile from the sensor
     * @param profileData Buffer to store the 22 bytes
     * @return true if successful
     */
    bool getCalibrationProfile(uint8_t* profileData);

    /**
     * @brief Writes a 22-byte calibration profile to the sensor
     * @param profileData 22-byte profile buffer
     * @return true if successful
     */
    bool setCalibrationProfile(const uint8_t* profileData);
}
