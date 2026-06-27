#include "ble_manager.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

static const char* TAG = "BLE";

/* Global pointer so static C callbacks can reach the instance */
static BLEManager* s_bleManager = nullptr;

/*=========================================================
      UUID helpers — convert string UUIDs to ble_uuid128_t
=========================================================*/

static ble_uuid128_t uuidFromString(const char* str)
{
    ble_uuid128_t uuid;
    uuid.u.type = BLE_UUID_TYPE_128;

    /* Parse 128-bit UUID string "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
       into little-endian byte array */
    uint8_t buf[16];
    int idx = 15;
    const char* p = str;
    while (*p && idx >= 0)
    {
        if (*p == '-') { p++; continue; }
        char hex[3] = { p[0], p[1], '\0' };
        buf[idx--] = (uint8_t)strtoul(hex, nullptr, 16);
        p += 2;
    }
    memcpy(uuid.value, buf, 16);
    return uuid;
}

static ble_uuid128_t s_serviceUuid;
static ble_uuid128_t s_commandCharUuid;
static ble_uuid128_t s_dataCharUuid;
static ble_uuid128_t s_batteryCharUuid;

/*=========================================================
              GATT Service Definition
=========================================================*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const struct ble_gatt_chr_def s_characteristics[] = {
    {
        .uuid = &s_commandCharUuid.u,
        .access_cb = BLEManager::gattAccessHandler,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {
        .uuid = &s_dataCharUuid.u,
        .access_cb = BLEManager::gattAccessHandler,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = &s_bleManager->dataValHandle,
        .cpfd = nullptr,
    },
    {
        .uuid = &s_batteryCharUuid.u,
        .access_cb = BLEManager::gattAccessHandler,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = &s_bleManager->batteryValHandle,
        .cpfd = nullptr,
    },
    { 0 },  /* Terminator */
};

const struct ble_gatt_svc_def BLEManager::gattServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_serviceUuid.u,
        .includes = nullptr,
        .characteristics = s_characteristics,
    },
    { 0 },  /* Terminator */
};

#pragma GCC diagnostic pop

/*=========================================================
              GAP Event Handler
=========================================================*/

int BLEManager::gapEventHandler(struct ble_gap_event* event, void* arg)
{
    BLEManager* mgr = s_bleManager;

    switch (event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0)
            {
                ESP_LOGI(TAG, "Device Connected");
                if (mgr)
                {
                    mgr->connected = true;
                    mgr->connHandle = event->connect.conn_handle;

                    /* Request low-power connection parameters */
                    struct ble_gap_upd_params params = {};
                    params.itvl_min = 80;    /* 100ms (80 * 1.25ms) */
                    params.itvl_max = 160;   /* 200ms (160 * 1.25ms) */
                    params.latency = 4;      /* Skip up to 4 connection events */
                    params.supervision_timeout = 500;  /* 5s (500 * 10ms) */
                    ble_gap_update_params(event->connect.conn_handle, &params);
                }
            }
            else
            {
                ESP_LOGW(TAG, "Connection failed, status=%d", event->connect.status);
                if (mgr) mgr->startAdvertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Device Disconnected");
            if (mgr)
            {
                mgr->connected = false;
                mgr->connHandle = 0;
                mgr->startAdvertising();
            }
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated: %d", event->mtu.value);
            break;

        case BLE_GAP_EVENT_PASSKEY_ACTION:
            if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
                struct ble_sm_io pk;
                memset(&pk, 0, sizeof(pk));
                pk.action = event->passkey.params.action;
                pk.passkey = 123456;
                ble_sm_inject_io(event->passkey.conn_handle, &pk);
                ESP_LOGI(TAG, "BLE Passkey generated: 123456");
            }
            break;

        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                ESP_LOGI(TAG, "Connection encrypted successfully");
            } else {
                ESP_LOGW(TAG, "Connection encryption failed: %d", event->enc_change.status);
            }
            break;

        default:
            break;
    }

    return 0;
}

/*=========================================================
              GATT Access Handler (writes)
=========================================================*/

int BLEManager::gattAccessHandler(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt* ctxt, void* arg)
{
    BLEManager* mgr = s_bleManager;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        if (!mgr) return BLE_ATT_ERR_UNLIKELY;

        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len == 0) return 0;

        uint8_t buf[256];
        uint16_t copyLen = (len < sizeof(buf)) ? len : sizeof(buf);
        os_mbuf_copydata(ctxt->om, 0, copyLen, buf);

        CommandPacket packet;
        packet.command = static_cast<Command>(buf[0]);
        packet.length = 0;
        memset(packet.payload, 0, sizeof(packet.payload));

        if (copyLen > 1)
        {
            packet.length = buf[1];

            uint8_t maxCopy = sizeof(packet.payload);
            uint8_t available = (copyLen > 2) ? (copyLen - 2) : 0;
            uint8_t toCopy = packet.length;
            if (toCopy > maxCopy) toCopy = maxCopy;
            if (toCopy > available) toCopy = available;

            if (toCopy > 0)
            {
                memcpy(packet.payload, buf + 2, toCopy);
            }
        }

        mgr->latestPacket = packet;
        mgr->commandAvailable = true;

        /* Wake the main loop from light sleep */
        xSemaphoreGive(mgr->commandSemaphore);

        ESP_LOGI(TAG, "Command Received: 0x%02X", (unsigned)buf[0]);
    }

    return 0;
}

/*=========================================================
              NimBLE Host Task & Callbacks
=========================================================*/

void BLEManager::onSync()
{
    /* Ensure we have a proper address */
    ble_hs_util_ensure_addr(0);

    if (s_bleManager)
    {
        s_bleManager->startAdvertising();
    }
}

void BLEManager::onReset(int reason)
{
    ESP_LOGW(TAG, "NimBLE host reset, reason=%d", reason);
}

void BLEManager::nimbleHostTask(void* param)
{
    /* This function runs the NimBLE host event loop.
       It returns only when nimble_port_stop() is called. */
    nimble_port_run();

    /* Clean up */
    nimble_port_freertos_deinit();
}

/*=========================================================
              BLEManager Implementation
=========================================================*/

BLEManager::BLEManager()
{
    connected = false;
    commandAvailable = false;
    latestPacket = {};
    connHandle = 0;

    s_bleManager = this;
}

void BLEManager::begin()
{
    ESP_LOGI(TAG, "Initializing...");

    /* Initialize command semaphore for main loop wake */
    commandSemaphore = xSemaphoreCreateBinary();

    /* Parse UUIDs from string constants */
    s_serviceUuid = uuidFromString(SERVICE_UUID);
    s_commandCharUuid = uuidFromString(COMMAND_CHARACTERISTIC_UUID);
    s_dataCharUuid = uuidFromString(DATA_CHARACTERISTIC_UUID);
    s_batteryCharUuid = uuidFromString(BATTERY_CHARACTERISTIC_UUID);

    /* Initialize NimBLE */
    nimble_port_init();

    /* Configure the NimBLE host */
    ble_hs_cfg.reset_cb = onReset;
    ble_hs_cfg.sync_cb = onSync;
    
    /* Configure NimBLE Security Manager Protocol (SMP) */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = 1; // Enc Key
    ble_hs_cfg.sm_their_key_dist = 1;

    /* Set preferred MTU */
    ble_att_set_preferred_mtu(BLE_MTU);

    /* Initialize GAP and GATT services */
    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* Register our custom GATT services */
    int rc = ble_gatts_count_cfg(gattServices);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(gattServices);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return;
    }

    /* Set device name */
    ble_svc_gap_device_name_set(DEVICE_NAME);

    /* Start the NimBLE host task */
    nimble_port_freertos_init(nimbleHostTask);

    ESP_LOGI(TAG, "Ready");
}

void BLEManager::configureAdvertising()
{

    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    /* Include device name */
    const char* name = ble_svc_gap_device_name();
    fields.name = (uint8_t*)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    /* Set service UUID in scan response */
    struct ble_hs_adv_fields rsp_fields = {};
    rsp_fields.uuids128 = &s_serviceUuid;
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;

    ble_gap_adv_rsp_set_fields(&rsp_fields);
}

void BLEManager::startAdvertising()
{
    if (ble_gap_adv_active()) return;

    configureAdvertising();

    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER,
                                &adv_params, gapEventHandler, nullptr);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Advertising start failed: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertising Started");
}

void BLEManager::stopAdvertising()
{
    ble_gap_adv_stop();
    ESP_LOGI(TAG, "Advertising Stopped");
}

void BLEManager::disconnect()
{
    if (connected && connHandle != 0)
    {
        ESP_LOGI(TAG, "Forcing disconnect...");
        ble_gap_terminate(connHandle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

bool BLEManager::isConnected() const
{
    return connected;
}

bool BLEManager::hasNewCommand()
{
    return commandAvailable;
}

CommandPacket BLEManager::getCommand()
{
    commandAvailable = false;
    return latestPacket;
}

bool BLEManager::waitForCommand(TickType_t timeout)
{
    return xSemaphoreTake(commandSemaphore, timeout) == pdTRUE;
}

void BLEManager::notifyData(const uint8_t* data, size_t length)
{
    if (!connected || connHandle == 0 || length == 0) return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
    if (!om) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for notification");
        return;
    }

    int rc = ble_gatts_notify_custom(connHandle, dataValHandle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send notification: %d", rc);
    }
}

void BLEManager::notifyBattery(uint8_t percentage)
{
    if (!connected || connHandle == 0) return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&percentage, sizeof(percentage));
    if (!om) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for battery notification");
        return;
    }

    int rc = ble_gatts_notify_custom(connHandle, batteryValHandle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send battery notification: %d", rc);
    }
}