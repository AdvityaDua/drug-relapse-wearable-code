#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#include "commands.h"
#include "types.h"

class BLEManager;
class PowerManager;

class CommandManager
{
public:
    CommandManager();

    void begin();

    void processPending(BLEManager& ble, PowerManager& power);

    bool isCollecting() const;

    uint32_t getSampleIntervalMs() const;

private:
    StatusCode executeCommand(const CommandPacket& packet, PowerManager& power, BLEManager& ble);

    void logCommandResult(uint8_t rawCommand, StatusCode status) const;

    bool collecting;
    uint32_t sampleIntervalMs;
};

#endif