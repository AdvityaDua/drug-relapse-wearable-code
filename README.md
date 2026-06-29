# Health Wearable Firmware (ESP32-C6)

This repository contains the complete firmware for an advanced, ultra-low-power health monitoring wearable built on the **Seeed Studio XIAO ESP32-C6**. 

The firmware is built using **PlatformIO** and **ESP-IDF**, employing FreeRTOS for scheduling, LittleFS for non-volatile storage, and NimBLE for a highly secure Bluetooth Low Energy (BLE) stack.

---

## 🏗 System Architecture

### 1. Hardware & Sensors
The system interfaces with multiple health and motion sensors over a shared I2C bus:
- **BNO055 (IMU):** 9-DOF sensor providing Euler angles, Quaternions, Linear Acceleration, Gravity, and raw vector data. Features automatic calibration persistence.
- **MAX30102 (PPG):** Optical sensor measuring Heart Rate (bpm) and Blood Oxygen saturation (SpO2) using Maxim's built-in algorithmic processing.
- **MAX30205 (Temperature):** Clinical-grade body temperature sensor.
- **TLA2022 (ADC for GSR):** 12-bit ADC reading analog Galvanic Skin Response (GSR) data.

*Note: The sensors are power-gated via a dedicated GPIO pin (Bus Low Enable) to ensure absolute zero power draw when the device enters deep sleep.*

### 2. Software Modules

The codebase is strictly modularized within the `src/` directory:

- **`sensors/`**: Houses the `SensorManager` and individual C++ driver wrappers. It handles power-cycling the sensors, initializing the I2C bus, grabbing synchronized readings, and generating the final JSON string.
- **`power/`**: Handles deep/light sleep transitions, GPIO wake-up interrupts, and calculates the internal battery percentage using the ESP32-C6's `esp_adc_oneshot` driver.
- **`storage/`**: The `StorageManager` mounts a LittleFS partition. It is responsible for appending historic JSON data logs during offline collection, streaming the logs over BLE on demand, and safely erasing them afterward.
- **`ble/`**: The `BLEManager` implements a NimBLE stack. It handles advertising, MTU negotiation, secure passkey bonding (SMP), and exposes custom GATT characteristics for bi-directional communication.
- **`commands/`**: The `CommandManager` parses inbound 1-byte command prefixes from the mobile app and orchestrates the resulting actions (e.g., initiating data sync, modifying sampling rates, setting internal time).

### 3. Execution Flow & Power Management
1. **Boot**: The ESP32 wakes up, initializes NVS, and restores the BNO055 calibration profile from LittleFS.
2. **Idle**: The BLE stack is brought online. If no commands are executing, the FreeRTOS idle task automatically drops the ESP32-C6 into a power-saving Light Sleep mode.
3. **Autonomous Collection Task**: An RTOS task runs strictly on a predefined interval (e.g., 1 second). It temporarily wakes the system, pulses power to the sensors, logs a timestamped JSON reading to LittleFS, and immediately returns to sleep.

---

## 📡 BLE Command Protocol

The wearable operates as a BLE Peripheral. It advertises a primary custom service and three characteristics. 

- **Service UUID:** `8A6F0001-5C2E-4C89-8C67-3E6E52E10001`
- **Command Characteristic (Write):** `...0002`
- **Data Characteristic (Notify):** `...0003`
- **Battery Characteristic (Read/Notify):** `...0005`

### Security
The device uses Secure Connections with MITM protection. Upon connecting, the Central (Phone) will be prompted to enter the fixed passkey: **`123456`**.

### Available Commands
Commands are written as binary payloads to the **Command Characteristic**. The first byte is the Command ID.

| Command ID | Name | Payload | Description |
| :---: | :--- | :--- | :--- |
| `0x01` | **START_COLLECTION** | None | Starts the autonomous background data collection loop. |
| `0x02` | **STOP_COLLECTION** | None | Stops the background collection loop. |
| `0x03` | **SYNC_DATA** | None | Triggers the device to stream all historic data stored in LittleFS over the Data Characteristic, then clears the flash. |
| `0x04` | **GET_STATUS** | None | Queries if the device is currently collecting. |
| `0x05` | **GET_BATTERY** | None | Triggers an immediate battery % reading on the Battery Characteristic. |
| `0x06` | **GET_LIVE_DATA** | None | Bypasses flash storage and instantly takes a reading, pushing the JSON string to the Data Characteristic. |
| `0x07` | **SET_TIME** | 4-byte `uint32` | Injects the current Unix Epoch time. Crucial for syncing historic data timestamps. |
| `0x08` | **RESTART_DEVICE** | None | Triggers a software reset of the ESP32. |
| `0x09` | **SET_SAMPLE_INTERVAL**| 4-byte `uint32` | Sets the delay (in milliseconds) between autonomous collection cycles. |

## 🛠 Building & Flashing

This project requires PlatformIO.

1. Connect the Seeed XIAO ESP32-C6 via USB-C.
2. Compile and upload the firmware:
   ```bash
   pio run -t upload
   ```
3. Monitor the serial output:
   ```bash
   pio device monitor
   ```
