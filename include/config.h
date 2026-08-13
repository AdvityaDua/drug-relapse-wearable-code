#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

/*=========================================================
                    DEVICE INFORMATION
=========================================================*/

constexpr char DEVICE_NAME[] = "Nudge Band";

/*=========================================================
                    GPIO CONFIGURATION
=========================================================*/

// Power Button (D2) — connects to GND when pressed
constexpr gpio_num_t PIN_BUTTON = GPIO_NUM_2;
constexpr uint32_t LONG_PRESS_DURATION_MS = 2000; // 2 seconds to toggle power

// --------------------------------------------------------
// I2C Configuration
// --------------------------------------------------------
#define I2C_MASTER_SCL_IO                                                      \
  16 /*!< GPIO number used for I2C master clock (Physical Pin D6) */
#define I2C_MASTER_SDA_IO                                                      \
  17 /*!< GPIO number used for I2C master data  (Physical Pin D7) */
#define I2C_MASTER_NUM                                                         \
  0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces   \
       available will depend on the chip */
#define I2C_MASTER_FREQ_HZ 400000   /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

// Sensor Power Enable (Physical Pin D3)
constexpr gpio_num_t PIN_SENSOR_POWER = GPIO_NUM_21;

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

constexpr uint64_t LIGHT_SLEEP_TIMEOUT_US = 30 * 1000000ULL; // 30 seconds

#endif