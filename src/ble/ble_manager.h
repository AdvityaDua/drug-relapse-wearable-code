#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "config.h"
#include "ble_uuid.h"
#include "commands.h"

/* ESP-IDF NimBLE headers */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

class BLEManager
{
public:

    BLEManager();

    void begin();

    void startAdvertising();

    void stopAdvertising();

    void disconnect();

    bool isConnected() const;

    bool hasNewCommand();

    CommandPacket getCommand();

    bool waitForCommand(TickType_t timeout);

    /**
     * @brief Notifies connected client with a chunk of data.
     */
    void notifyData(const uint8_t* data, size_t length);

    /**
     * @brief Notifies connected client with battery percentage.
     */
    void notifyBattery(uint8_t percentage);

    uint16_t dataValHandle;
    uint16_t batteryValHandle;

    /* NimBLE callbacks (static, required by C API) */
    static int gapEventHandler(struct ble_gap_event* event, void* arg);
    static int gattAccessHandler(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt* ctxt, void* arg);

    /* GATT service/characteristic definition */
    static const struct ble_gatt_svc_def gattServices[];

private:

    std::atomic<bool> connected;

    std::atomic<bool> commandAvailable;

    CommandPacket latestPacket;

    uint16_t connHandle;

    SemaphoreHandle_t commandSemaphore;

    /* Internal helpers */
    void configureAdvertising();
    static void nimbleHostTask(void* param);
    static void onSync();
    static void onReset(int reason);
};

#endif