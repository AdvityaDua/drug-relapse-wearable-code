#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

/*
----------------------------------------------------
Generic Status Codes
----------------------------------------------------
*/

enum class StatusCode : uint8_t
{
    SUCCESS = 0,
    ERROR,
    BUSY,
    INVALID_COMMAND,
    BLE_DISCONNECTED,
    NOT_IMPLEMENTED
};

/*
----------------------------------------------------
Power State
----------------------------------------------------
*/

enum class PowerState : uint8_t
{
    ACTIVE,
    LIGHT_SLEEP,
    DEEP_SLEEP
};

/*
----------------------------------------------------
Current Device State
----------------------------------------------------
*/

struct DeviceState
{
    bool bleConnected = false;
    bool sensorsPowered = false;
    bool collecting = false;

    PowerState powerState = PowerState::ACTIVE;
};

#endif