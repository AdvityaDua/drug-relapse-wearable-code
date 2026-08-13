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

    // Button monitoring — spawns a background task that polls D2
    void startButtonMonitor();

    // Returns true if a long-press was detected and shutdown is requested
    bool isShutdownRequested() const;

    // Sleep
    void enterDeepSleep();

    // Sensor Power
    void setSensorPower(bool enabled);
    void enableSensorPower();
    void disableSensorPower();

    // Status
    PowerState getPowerState() const;
    esp_sleep_wakeup_cause_t getWakeupCause();
    uint8_t getBatteryPercentage();

private:
    PowerState currentState;
    bool adcInitialized = false;
    volatile bool shutdownRequested = false;

    static void buttonMonitorTask(void* pvParameters);
};

#endif