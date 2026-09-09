#include "../../include/config.h"
#if USE_WIFI
#include "wifi_manager.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"

#include <cstring>
#include <algorithm>



static const char *TAG = "WIFI";
static WiFiManager* s_wifiManager = nullptr;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

WiFiManager::WiFiManager()
{
    clientConnected = false;
    activeClientSock = -1;
    commandAvailable = false;
    latestPacket = {};
    s_wifiManager = this;
}

void WiFiManager::begin()
{
    ESP_LOGI(TAG, "Initializing WiFi AP mode...");

    commandSemaphore = xSemaphoreCreateBinary();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {};
    memcpy(wifi_config.ap.ssid, WIFI_AP_SSID, strlen(WIFI_AP_SSID));
    wifi_config.ap.ssid_len = strlen(WIFI_AP_SSID);
    memcpy(wifi_config.ap.password, WIFI_AP_PASS, strlen(WIFI_AP_PASS));
    wifi_config.ap.max_connection = WIFI_MAX_CONNECTIONS;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    if (strlen(WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi SoftAP started. SSID: %s", WIFI_AP_SSID);

    xTaskCreate(tcpServerTask, "tcp_server", 4096, this, 5, NULL);
}

bool WiFiManager::hasNewCommand()
{
    return commandAvailable;
}

CommandPacket WiFiManager::getCommand()
{
    commandAvailable = false;
    return latestPacket;
}

bool WiFiManager::waitForCommand(TickType_t timeout)
{
    return xSemaphoreTake(commandSemaphore, timeout) == pdTRUE;
}

bool WiFiManager::notifyData(const uint8_t* data, size_t length)
{
    if (clientConnected && activeClientSock != -1) {
        int to_write = length;
        int written = 0;
        while (to_write > 0) {
            int err = send(activeClientSock, data + written, to_write, 0);
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                return false;
            }
            to_write -= err;
            written += err;
        }
        return true;
    }
    return false;
}

void WiFiManager::notifyBattery(uint8_t percentage)
{
    notifyData(&percentage, sizeof(percentage));
}

bool WiFiManager::isConnected() const
{
    return clientConnected;
}

void WiFiManager::tcpServerTask(void* param)
{
    WiFiManager* self = static_cast<WiFiManager*>(param);

    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    struct sockaddr_storage dest_addr;

    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(WIFI_TCP_PORT);
    ip_protocol = IPPROTO_IP;

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    ESP_LOGI(TAG, "Socket listening on port %d", WIFI_TCP_PORT);

    while (1) {
        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);

        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }

        ESP_LOGI(TAG, "TCP Client Connected: %s", addr_str);
        self->handleClient(sock);
        ESP_LOGI(TAG, "TCP Client Disconnected.");
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void WiFiManager::handleClient(int sock)
{
    activeClientSock = sock;
    clientConnected = true;

    uint8_t rx_buffer[256];

    while (1) {
        int len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
        if (len < 0) {
            ESP_LOGE(TAG, "recv failed: errno %d", errno);
            break;
        } else if (len == 0) {
            ESP_LOGI(TAG, "Connection closed");
            break;
        } else {
            // Process Command Packet
            CommandPacket packet;
            packet.command = static_cast<Command>(rx_buffer[0]);
            packet.length = 0;
            memset(packet.payload, 0, sizeof(packet.payload));

            if (len > 1) {
                packet.length = rx_buffer[1];

                uint8_t maxCopy = sizeof(packet.payload);
                uint8_t available = (len > 2) ? (len - 2) : 0;
                uint8_t toCopy = packet.length;
                if (toCopy > maxCopy) toCopy = maxCopy;
                if (toCopy > available) toCopy = available;

                if (toCopy > 0) {
                    memcpy(packet.payload, rx_buffer + 2, toCopy);
                }
            }

            latestPacket = packet;
            commandAvailable = true;
            xSemaphoreGive(commandSemaphore);
            ESP_LOGI(TAG, "Command Received: 0x%02X", (unsigned)rx_buffer[0]);
        }
    }

    clientConnected = false;
    activeClientSock = -1;
    close(sock);
}
#endif // USE_WIFI
