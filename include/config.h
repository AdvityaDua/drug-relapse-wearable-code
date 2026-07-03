#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

/*=========================================================
                    DEVICE INFORMATION
=========================================================*/

constexpr char DEVICE_NAME[] = "Health Wearable";

/*=========================================================
                    GPIO CONFIGURATION
=========================================================*/

// Bus Low Enable jumper
constexpr gpio_num_t PIN_BUS_LOW = GPIO_NUM_2;

// --------------------------------------------------------
// I2C Configuration
// --------------------------------------------------------
#define I2C_MASTER_SCL_IO           7               /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           6               /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000          /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0               /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0               /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

// --------------------------------------------------------
// System Settings
// --------------------------------------------------------
// Wake when jumper is installed (GPIO pulled LOW)
constexpr int BUS_LOW_WAKE_LEVEL = 0;

// Sensor Power Enable
constexpr gpio_num_t PIN_SENSOR_POWER = GPIO_NUM_3;

// Onboard LED (XIAO ESP32C6 built-in LED, active-low)
constexpr gpio_num_t PIN_LED = GPIO_NUM_15;

/*=========================================================
                    BLE CONFIGURATION
=========================================================*/

constexpr uint16_t BLE_MTU = 247;

/*=========================================================
                    BATTERY CONFIGURATION
=========================================================*/

constexpr gpio_num_t PIN_BATTERY = GPIO_NUM_0; // A0 on XIAO ESP32C6

constexpr float BATTERY_LOW_VOLTAGE = 3.50f;
constexpr float BATTERY_FULL_VOLTAGE = 4.20f;

/*=========================================================
                    SENSOR CONFIGURATION
=========================================================*/

constexpr uint32_t DEFAULT_SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t MIN_SAMPLE_INTERVAL_MS = 100;
constexpr uint32_t MAX_SAMPLE_INTERVAL_MS = 60000;

/*=========================================================
                    SLEEP CONFIGURATION
=========================================================*/

constexpr uint64_t LIGHT_SLEEP_TIMEOUT_US = 30 * 1000000ULL;  // 30 seconds

#endif