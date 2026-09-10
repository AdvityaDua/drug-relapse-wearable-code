#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#include "commands.h"
#include "types.h"

#include "config.h"

#if USE_WIFI
class WiFiManager;
typedef WiFiManager TransportManager;
#else
class BLEManager;
typedef BLEManager TransportManager;
#endif

class PowerManager;

class CommandManager
{
public:
    CommandManager();

    void begin();

    void processPending(TransportManager& transport, PowerManager& power);

    bool isCollecting() const;
    uint64_t getPatientId() const;

    uint32_t getSampleIntervalMs() const;

private:
    StatusCode executeCommand(const CommandPacket& packet, PowerManager& power, TransportManager& transport);

    void logCommandResult(uint8_t rawCommand, StatusCode status) const;

    bool collecting;
    uint64_t patientId;
    uint32_t sampleIntervalMs;
};

#endif