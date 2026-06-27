#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>
/*
----------------------------------------------------
Commands received from BLE
----------------------------------------------------
*/
enum class Command : uint8_t
{
    START_COLLECTION = 0x01,
    STOP_COLLECTION  = 0x02,

    SYNC_DATA        = 0x03,

    GET_STATUS       = 0x04,
    GET_BATTERY      = 0x05,

    GET_LIVE_DATA    = 0x06,
    SET_TIME         = 0x07,
    RESTART_DEVICE   = 0x08,

    SET_SAMPLE_INTERVAL = 0x09
};

namespace CommandUtils
{
inline bool isKnownCommand(uint8_t rawCommand)
{
    switch (rawCommand)
    {
        case static_cast<uint8_t>(Command::START_COLLECTION):
        case static_cast<uint8_t>(Command::STOP_COLLECTION):
        case static_cast<uint8_t>(Command::SYNC_DATA):
        case static_cast<uint8_t>(Command::GET_STATUS):
        case static_cast<uint8_t>(Command::GET_BATTERY):
        case static_cast<uint8_t>(Command::GET_LIVE_DATA):
        case static_cast<uint8_t>(Command::SET_TIME):
        case static_cast<uint8_t>(Command::RESTART_DEVICE):
        case static_cast<uint8_t>(Command::SET_SAMPLE_INTERVAL):
            return true;

        default:
            return false;
    }
}

inline const char* commandToString(uint8_t rawCommand)
{
    switch (rawCommand)
    {
        case static_cast<uint8_t>(Command::START_COLLECTION):
            return "START_COLLECTION";

        case static_cast<uint8_t>(Command::STOP_COLLECTION):
            return "STOP_COLLECTION";

        case static_cast<uint8_t>(Command::SYNC_DATA):
            return "SYNC_DATA";

        case static_cast<uint8_t>(Command::GET_STATUS):
            return "GET_STATUS";

        case static_cast<uint8_t>(Command::GET_BATTERY):
            return "GET_BATTERY";

        case static_cast<uint8_t>(Command::GET_LIVE_DATA):
            return "GET_LIVE_DATA";

        case static_cast<uint8_t>(Command::SET_TIME):
            return "SET_TIME";

        case static_cast<uint8_t>(Command::RESTART_DEVICE):
            return "RESTART_DEVICE";

        case static_cast<uint8_t>(Command::SET_SAMPLE_INTERVAL):
            return "SET_SAMPLE_INTERVAL";

        default:
            return "UNKNOWN_COMMAND";
    }
}
}
/*
----------------------------------------------------
BLE Command Packet
----------------------------------------------------
*/

struct CommandPacket
{
    Command command;
    uint8_t length;
    uint8_t payload[32];
};

#endif