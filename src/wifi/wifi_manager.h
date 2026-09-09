#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"

#include "commands.h"

class WiFiManager
{
public:
    WiFiManager();

    void begin();
    
    // Checks if the TCP server has received a new command packet
    bool hasNewCommand();
    
    // Returns the latest command packet
    CommandPacket getCommand();
    
    // Wait for a command for a certain amount of time
    bool waitForCommand(TickType_t timeout);

    // Write data to the active TCP client
    bool notifyData(const uint8_t* data, size_t length);
    void notifyBattery(uint8_t percentage);

    bool isConnected() const;

private:
    static void tcpServerTask(void* param);
    void handleClient(int sock);

    bool clientConnected;
    int activeClientSock;
    
    bool commandAvailable;
    CommandPacket latestPacket;
    SemaphoreHandle_t commandSemaphore;
};

#endif
