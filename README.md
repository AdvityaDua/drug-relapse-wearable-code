# Health Wearable Controller & Patient Session Manager

A modern, high-performance **Flutter** application built as a capstone project to interface with an ESP32-based health-monitoring wearable. The app manages patient records, handles secure Bluetooth Low Energy (BLE) pairing, runs real-time monitoring sessions, and provides tools for offline CSV data analysis and sharing.

---

##  Key Features

* **Advanced BLE Scanning & Pairing**:
  * Real-time permission handling (Bluetooth Scan, Connect, and Location) for Android 12+ and iOS.
  * Robust device discovery subscribing to advertisements natively before initiating scans.
  * Secure OS-level pairing using a custom passkey (`123456`).
  * Dynamic connection monitoring with auto-reconnect handling and UI banners.
* **Patient Management**:
  * Manage individual patient profiles.
  * Associate specific patient records with active monitoring sessions to ensure data integrity.
* **Automated Session Manager**:
  * Manages active **2-hour data collection sessions**.
  * Spawns background timers to request data synchronization every **15 minutes** (up to 8 sync cycles maximum per session).
  * Automatically stops collections and writes shutdown commands to the wearable once completed.
* **Real-time Vitals Dashboard**:
  * Renders live graphs/vitals (Heart Rate, SpO2, Body Temperature) from incoming BLE data streams.
  * Resolves raw JSON streams from the wearable dynamically.
* **Interactive CSV Preview & Share**:
  * Full spreadsheet-like interactive grid (powered by `PlutoGrid`) for reviewing recorded patient sensor histories.
  * Save CSV files locally on the device or share them externally using system dialogs.
* **Command Terminal Console**:
  * Built-in interactive ASCII terminal console to send raw string/text commands over BLE for debugging or custom operations.
  * Automatic fallback that identifies any writable and notify characteristics on standard third-party peripherals (e.g. phone simulators).

---

## Technology Stack

| Component | Package |
| :--- | :--- |
| **BLE Communication** | `flutter_blue_plus` |
| **State Management** | `flutter_riverpod` |
| **Navigation** | `go_router` |
| **Spreadsheet Grid** | `pluto_grid` |
| **CSV Parsing** | `csv` |
| **Storage Access** | `path_provider` |
| **File Sharing** | `share_plus` |
| **Permissions** | `permission_handler` |

---

##  BLE Command Protocol (UART ASCII)

The app communicates with the wearable's Nordic UART Service (NUS) using UTF-8 string messages terminated with a newline (`\n`).

| Hex Code | Command String | Description |
| :--- | :--- | :--- |
| `0x01` | `"1\n"` | **Start Collection**: Begins sensor acquisition and live streaming. |
| `0x02` | `"2\n"` | **Stop Collection**: Stops sensor acquisition. |
| `0x03` | `"3\n"` | **Sync Data**: Triggers sync of stored offline CSV log files. |
| `0x05` | `"5\n"` | **Get Battery**: Requests current wearable battery percentage. |
| `0x06` | `"6\n"` | **Get Live Data**: Requests immediate real-time sensor vitals frame. |

---

##  Project Architecture

The codebase adheres to a clean layered architecture separating concerns into:

```text
Presentation Layer (UI Screens)
   │
   ▼
Riverpod Providers (State Observers)
   │
   ▼
Business Layer (Session Manager / CSV Controller)
   │
   ▼
Data Layer (BleService / Local Storage)
   │
   ▼
flutter_blue_plus ──► ESP32 Wearable
```

---

## Getting Started

### Prerequisites

* [Flutter SDK](https://docs.flutter.dev/get-started/install) (version ^3.11.3)
* Xcode (for iOS deployment) / Android Studio (for Android deployment)

### Installation & Run

1. **Clone the repository**:
   ```bash
   git clone https://github.com/Yowan-Sharma/capstone.git
   cd capstone/health_wearable
   ```

2. **Install dependencies**:
   ```bash
   flutter pub get
   ```

3. **Run the tests**:
   Ensure all unit, logic, and widget tests pass:
   ```bash
   flutter test
   ```

4. **Launch the application**:
   * Run in debug mode:
     ```bash
     flutter run
     ```
   * Run in release mode on a connected iPhone:
     ```bash
     flutter run --release
     ```
