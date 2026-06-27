#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "driver/gpio.h"
#include "esp_sleep.h"

#include "config.h"
#include "types.h"

class PowerManager
{
public:
    PowerManager();

    // Initialization
    void begin();

    // Sleep
    void enterDeepSleep();

    // Sensor Power
    void setSensorPower(bool enabled);
    void enableSensorPower();
    void disableSensorPower();

    // Bus Low Enable
    bool isBusLowEnabled();
    bool shouldEnterDeepSleep();

    // Status
    PowerState getPowerState() const;
    esp_sleep_wakeup_cause_t getWakeupCause();
    uint8_t getBatteryPercentage();

private:
    PowerState currentState;
    bool adcInitialized = false;
};

#endif