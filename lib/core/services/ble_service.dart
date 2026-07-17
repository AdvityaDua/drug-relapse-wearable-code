import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import '../constants/commands.dart';
import '../constants/ble_uuids.dart';
import '../models/wearable_device.dart';

class BleService {
  /// Set to [false] when you want to use the real ESP32 wearable.
  bool isMock = false;

  // ── ESP32 Default BLE UUIDs (Nordic UART Service) ────────────────────────
  // Feel free to change these if your custom ESP32 firmware uses different UUIDs.
  static const String espServiceUuid = BleUuids.espServiceUuid;
  static const String espRxCharacteristicUuid = BleUuids.espRxCharacteristicUuid; // TX from Phone
  static const String espTxCharacteristicUuid = BleUuids.espTxCharacteristicUuid; // RX to Phone

  // ── Scanned devices ──────────────────────────────────────────────────────
  final StreamController<List<WearableDevice>> _devicesController =
      StreamController.broadcast();
  Stream<List<WearableDevice>> get scannedDevices => _devicesController.stream;

  // ── Scanning state ────────────────────────────────────────────────────────
  final StreamController<bool> _isScanningController =
      StreamController.broadcast();
  Stream<bool> get isScanning => _isScanningController.stream;

  // ── Battery (stable – only updated on connect or real notification) ───────
  final StreamController<int> _batteryController =
      StreamController.broadcast();
  Stream<int> get batteryStream => _batteryController.stream;

  int? _batteryLevel; // cached value so it doesn't flicker
  int? get batteryLevel => _batteryLevel;

  // ── Live sensor data ──────────────────────────────────────────────────────
  final StreamController<String> _liveDataController =
      StreamController.broadcast();
  Stream<String> get liveDataStream => _liveDataController.stream;

  // ── CSV data chunks ───────────────────────────────────────────────────────
  final StreamController<String> _csvDataController =
      StreamController.broadcast();
  Stream<String> get csvDataStream => _csvDataController.stream;

  // ── Connection state ──────────────────────────────────────────────────────
  final StreamController<bool> _connectionStateController =
      StreamController.broadcast();
  Stream<bool> get connectionStateStream => _connectionStateController.stream;

  bool _connected = false;
  bool get isConnected => _connected;

  // Real BLE subscriptions (used when isMock == false)
  StreamSubscription<List<ScanResult>>? _scanSubscription;
  StreamSubscription<BluetoothConnectionState>? _connectionStateSub;
  BluetoothDevice? _bleDevice;
  String? _connectedDeviceId;
  BluetoothCharacteristic? _writeCharacteristic;

  // Mock Mode Timers
  Timer? _mockLiveDataTimer;

  // ─────────────────────────────────────────────────────────────────────────
  // PERMISSIONS HELPER
  // ─────────────────────────────────────────────────────────────────────────
  Future<bool> requestPermissions() async {
    if (Platform.isAndroid) {
      final statuses = await [
        Permission.bluetoothScan,
        Permission.bluetoothConnect,
        Permission.location,
      ].request();
      
      return statuses[Permission.bluetoothScan] == PermissionStatus.granted &&
          statuses[Permission.bluetoothConnect] == PermissionStatus.granted &&
          statuses[Permission.location] == PermissionStatus.granted;
    } else if (Platform.isIOS) {
      // iOS Bluetooth central permission is handled natively by CoreBluetooth 
      // when accessing Bluetooth APIs (like startScan). We do not need to block
      // on permission_handler checks here.
      return true;
    }
    return true;
  }

  // ─────────────────────────────────────────────────────────────────────────
  // SCANNING
  // ─────────────────────────────────────────────────────────────────────────
  Future<void> startScan() async {
    if (isMock) {
      _isScanningController.add(true);
      _devicesController.add([]); // clear previous results

      // After 2 seconds, emit a fake ESP32 device in the list
      await Future.delayed(const Duration(seconds: 2));

      _devicesController.add([
        WearableDevice(
          id: 'MOCK_ESP32_001',
          name: 'ESP32 Wearable',
          rssi: -62,
        ),
      ]);
      _isScanningController.add(false);
    } else {
      // Check and request runtime permissions
      final permissionsGranted = await requestPermissions();
      if (!permissionsGranted) {
        throw Exception("Bluetooth and Location permissions are required for BLE scanning.");
      }

      // Check Bluetooth adapter state
      if (await FlutterBluePlus.adapterState.first != BluetoothAdapterState.on) {
        try {
          if (Platform.isAndroid) {
            await FlutterBluePlus.turnOn();
          }
        } catch (_) {}
        await Future.delayed(const Duration(milliseconds: 500));
        if (await FlutterBluePlus.adapterState.first != BluetoothAdapterState.on) {
          throw Exception("Bluetooth adapter is off. Please enable Bluetooth.");
        }
      }

      // Real scanning via flutter_blue_plus
      final List<WearableDevice> found = [];
      _isScanningController.add(true);
      _devicesController.add([]);

      try {
        await _scanSubscription?.cancel();
        
        // 1. Setup subscription BEFORE starting the scan so we don't miss results
        _scanSubscription = FlutterBluePlus.scanResults.listen((results) {
          found.clear();
          for (final r in results) {
            String name = r.advertisementData.advName;
            if (name.isEmpty) {
              name = r.device.platformName;
            }
            
            if (name.isEmpty) {
              // Try to identify if it's the ESP32 based on service UUIDs
              bool isEsp = r.advertisementData.serviceUuids.any((uuid) => 
                  uuid.toString().toLowerCase() == espServiceUuid.toLowerCase());
              
              name = isEsp ? 'ESP32 (Hidden Name)' : 'Unknown Device';
            }

            found.add(WearableDevice(
              id: r.device.remoteId.str,
              name: name,
              rssi: r.rssi,
            ));
          }
          _devicesController.add(List.from(found));
        });

        // 2. Setup scanning state listener
        FlutterBluePlus.isScanning.listen((scanning) {
          _isScanningController.add(scanning);
        });

        // 3. Trigger the scan
        await FlutterBluePlus.startScan(timeout: const Duration(seconds: 5));
      } catch (e) {
        _isScanningController.add(false);
        rethrow;
      }
    }
  }

  Future<void> stopScan() async {
    if (isMock) {
      _isScanningController.add(false);
    } else {
      await FlutterBluePlus.stopScan();
      await _scanSubscription?.cancel();
      _scanSubscription = null;
    }
  }

  // ─────────────────────────────────────────────────────────────────────────
  // CONNECT / DISCONNECT
  // ─────────────────────────────────────────────────────────────────────────
  Future<bool> connect(String deviceId) async {
    if (isMock) {
      await Future.delayed(const Duration(seconds: 1));
      _connected = true;
      _connectedDeviceId = deviceId;
      _connectionStateController.add(true);

      // Emit a stable battery level once (no fluctuating timer)
      _batteryLevel = 82;
      _batteryController.add(_batteryLevel!);
      return true;
    } else {
      // Real connection
      final target = BluetoothDevice(remoteId: DeviceIdentifier(deviceId));
      try {
        await _connectionStateSub?.cancel();

        // Listen for dynamic connection state changes
        _connectionStateSub = target.connectionState.listen((state) {
          final isNowConnected = state == BluetoothConnectionState.connected;
          if (_connected != isNowConnected) {
            _connected = isNowConnected;
            _connectionStateController.add(isNowConnected);
            if (!isNowConnected) {
              _connectedDeviceId = null;
              _writeCharacteristic = null;
              _batteryLevel = null;
            }
          }
        });

        await target.connect(license: License.nonprofit);
        _bleDevice = target;
        _connected = true;
        _connectedDeviceId = deviceId;
        _connectionStateController.add(true);

        // Request pairing/bonding on Android if needed
        if (Platform.isAndroid) {
          try {
            await target.createBond();
          } catch (_) {}
        }

        await _fetchRealBattery();
        await _subscribeToRealData();
        return true;
      } catch (_) {
        _connected = false;
        _connectionStateController.add(false);
        return false;
      }
    }
  }

  Future<void> disconnect() async {
    if (isMock) {
      _connected = false;
      _connectedDeviceId = null;
      _batteryLevel = null;
      _mockLiveDataTimer?.cancel();
      _connectionStateController.add(false);
    } else {
      await _bleDevice?.disconnect();
      _bleDevice = null;
      _connected = false;
      _connectedDeviceId = null;
      _batteryLevel = null;
      _writeCharacteristic = null;
      await _connectionStateSub?.cancel();
      _connectionStateSub = null;
      _connectionStateController.add(false);
    }
  }

  // ─────────────────────────────────────────────────────────────────────────
  // COMMANDS
  // ─────────────────────────────────────────────────────────────────────────
  Future<void> writeString(String text) async {
    if (isMock) {
      print("Mock writeString: $text");
      if (text.trim() == "1") {
        writeCommand(BleCommands.startCollection);
      } else if (text.trim() == "2") {
        writeCommand(BleCommands.stopCollection);
      } else if (text.trim() == "3") {
        writeCommand(BleCommands.syncData);
      }
    } else {
      if (_writeCharacteristic != null) {
        try {
          await _writeCharacteristic!.write(utf8.encode(text));
        } catch (_) {
          // Ignore command failures if device disconnects suddenly
        }
      }
    }
  }

  Future<void> writeCommand(int command) async {
    if (isMock) {
      switch (command) {
        case BleCommands.startCollection:
          // Mock: collection started
          _mockLiveDataTimer?.cancel();
          _mockLiveDataTimer = Timer.periodic(const Duration(seconds: 1), (timer) {
            writeCommand(BleCommands.getLiveData);
          });
          break;
        case BleCommands.stopCollection:
          // Mock: collection stopped
          _mockLiveDataTimer?.cancel();
          break;
        case BleCommands.syncData:
          await Future.delayed(const Duration(seconds: 1));
          final ts = DateTime.now().millisecondsSinceEpoch ~/ 1000;
          final header = 'time,gsr,bodyTemp,hr,validHR,spo2,validSPO2,bno055_euler_heading,bno055_euler_roll,bno055_euler_pitch,bno055_quat_w,bno055_quat_x,bno055_quat_y,bno055_quat_z,bno055_linear_x,bno055_linear_y,bno055_linear_z,bno055_gravity_x,bno055_gravity_y,bno055_gravity_z,bno055_accel_x,bno055_accel_y,bno055_accel_z,bno055_gyro_x,bno055_gyro_y,bno055_gyro_z,bno055_mag_x,bno055_mag_y,bno055_mag_z,bno055_temp,bno055_calib_sys,bno055_calib_gyro,bno055_calib_accel,bno055_calib_mag\n';
          String row1 = '$ts,500,36.5,72,1,98,1,45.0,0.5,1.2,1.0,0.0,0.0,0.0,0.1,0.2,9.8,0.0,0.0,9.81,0.1,0.2,9.8,0.01,0.02,0.01,20.0,30.0,40.0,28,3,3,3,3\n';
          String row2 = '${ts + 1},505,36.6,73,1,98,1,45.1,0.6,1.3,0.9,0.1,0.1,0.0,0.2,0.1,9.7,0.0,0.0,9.81,0.2,0.1,9.7,0.02,0.01,0.02,20.1,30.1,40.1,28,3,3,3,3\n';
          String row3 = '${ts + 2},495,36.4,71,1,97,1,44.9,0.4,1.1,0.9,0.1,0.0,0.1,0.0,0.3,9.9,0.0,0.0,9.81,0.0,0.3,9.9,0.01,0.02,0.00,19.9,29.9,39.9,28,3,3,3,3';
          _csvDataController.add(header + row1 + row2 + row3);
          break;
        case BleCommands.getLiveData:
          final rng = Random();
          // Emit a JSON string for easy parsing in LiveDataScreen
          _liveDataController.add(
            '{"time": ${DateTime.now().millisecondsSinceEpoch ~/ 1000}, "gsr": ${500 + rng.nextInt(20)}, "bodyTemp": 36.5, "hr": ${70 + rng.nextInt(10)}, "validHR": 1, "spo2": ${95 + rng.nextInt(5)}, "validSPO2": 1, "bno055_euler_heading": 45.0, "bno055_euler_roll": 0.5, "bno055_euler_pitch": 1.2, "bno055_quat_w": 1.0, "bno055_quat_x": 0.0, "bno055_quat_y": 0.0, "bno055_quat_z": 0.0, "bno055_linear_x": 0.1, "bno055_linear_y": 0.2, "bno055_linear_z": 9.8, "bno055_gravity_x": 0.0, "bno055_gravity_y": 0.0, "bno055_gravity_z": 9.81, "bno055_accel_x": 0.1, "bno055_accel_y": 0.2, "bno055_accel_z": 9.8, "bno055_gyro_x": 0.01, "bno055_gyro_y": 0.02, "bno055_gyro_z": 0.01, "bno055_mag_x": 20.0, "bno055_mag_y": 30.0, "bno055_mag_z": 40.0, "bno055_temp": 28, "bno055_calib_sys": 3, "bno055_calib_gyro": 3, "bno055_calib_accel": 3, "bno055_calib_mag": 3}'
          );
          break;
        case BleCommands.getBattery:
          // Refresh battery once (stable value, not random)
          if (_batteryLevel != null) {
            _batteryController.add(_batteryLevel!);
          }
          break;
      }
    } else {
      if (_writeCharacteristic != null) {
        try {
          // Send command as a raw byte. The ESP32 expects `buf[0]` to be the command enum value.
          await _writeCharacteristic!.write([command]);
        } catch (_) {
          // Ignore command failures if device disconnects suddenly
        }
      }
    }
  }

  // ─────────────────────────────────────────────────────────────────────────
  // REAL BLE HELPENS
  // ─────────────────────────────────────────────────────────────────────────
  Future<void> _fetchRealBattery() async {
    if (_bleDevice == null) return;
    try {
      final services = await _bleDevice!.discoverServices();
      for (final s in services) {
        // Battery Service UUID
        if (s.uuid.toString().toLowerCase() == BleUuids.batteryServiceUuid.toLowerCase()) {
          for (final c in s.characteristics) {
            // Battery Level UUID
            if (c.uuid.toString().toLowerCase() == BleUuids.batteryLevelCharacteristicUuid.toLowerCase()) {
              final value = await c.read();
              if (value.isNotEmpty) {
                _batteryLevel = value.first;
                _batteryController.add(_batteryLevel!);
              }
              // Subscribe to battery notifications
              await c.setNotifyValue(true);
              c.onValueReceived.listen((v) {
                if (v.isNotEmpty) {
                  _batteryLevel = v.first;
                  _batteryController.add(_batteryLevel!);
                }
              });
            }
          }
        }
      }
    } catch (_) {
      // Battery service not found or read failed – ignore
    }
  }

  Future<void> _subscribeToRealData() async {
    if (_bleDevice == null) return;
    try {
      final services = await _bleDevice!.discoverServices();
      print("========== DISCOVERED BLE SERVICES ==========");
      for (final s in services) {
        print("Service: ${s.uuid}");
        for (final c in s.characteristics) {
          print("  -> Characteristic: ${c.uuid} (Write: ${c.properties.write}, Notify: ${c.properties.notify})");
        }
      }
      print("=============================================");

      for (final s in services) {
        if (s.uuid.toString().toLowerCase() == BleUuids.espServiceUuid.toLowerCase()) {
          for (final c in s.characteristics) {
            final uuid = c.uuid.toString().toLowerCase();
            
            // RX characteristic (Phone writes commands here)
            if (uuid == BleUuids.espRxCharacteristicUuid.toLowerCase()) {
              _writeCharacteristic = c;
            }
            
            // TX characteristic (ESP32 sends data here)
            if (uuid == BleUuids.espTxCharacteristicUuid.toLowerCase()) {
              await c.setNotifyValue(true);
              c.onValueReceived.listen((value) {
                if (value.isNotEmpty) {
                  // Convert byte stream to string
                  final str = String.fromCharCodes(value);
                  
                  // If it's a JSON string, route to Live Data. Otherwise treat as CSV chunk.
                  if (str.trim().startsWith('{')) {
                    _liveDataController.add(str);
                  } else {
                    _csvDataController.add(str);
                  }
                }
              });
            }
          }
        }
      }

      // Fallback: If we didn't find the specific ESP32 UART write characteristic,
      // search all characteristics of all services for one that supports write.
      if (_writeCharacteristic == null) {
        for (final s in services) {
          for (final c in s.characteristics) {
            // Ignore standard 16-bit GATT UUIDs (which often start with 0000xxxx) to avoid writing to system features like 2B29
            bool isCustomUuid = c.uuid.toString().length > 8 && !c.uuid.toString().startsWith("0000");
            if (isCustomUuid && (c.properties.write || c.properties.writeWithoutResponse)) {
              print("WARNING: Falling back to writable characteristic ${c.uuid}");
              _writeCharacteristic = c;
              break;
            }
          }
          if (_writeCharacteristic != null) break;
        }
      }

      // Fallback: If we don't have a notification subscription active but we found a fallback TX, 
      // let's listen to the first characteristic that supports notify/indicate
      // and whose service is NOT the standard battery service (since we handle battery separately)
      bool hasNotifySubscribed = false;
      for (final s in services) {
        if (s.uuid.toString().toLowerCase() == BleUuids.batteryServiceUuid.toLowerCase()) continue;
        for (final c in s.characteristics) {
          if (c.uuid.toString().toLowerCase() == BleUuids.espTxCharacteristicUuid.toLowerCase()) {
            hasNotifySubscribed = true; // custom ESP TX is already subscribed
            break;
          }
        }
      }

      if (!hasNotifySubscribed) {
        for (final s in services) {
          if (s.uuid.toString().toLowerCase() == BleUuids.batteryServiceUuid.toLowerCase()) continue;
          for (final c in s.characteristics) {
            if (c.properties.notify || c.properties.indicate) {
              await c.setNotifyValue(true);
              c.onValueReceived.listen((value) {
                if (value.isNotEmpty) {
                  final str = String.fromCharCodes(value);
                  if (str.trim().startsWith('{')) {
                    _liveDataController.add(str);
                  } else {
                    _csvDataController.add(str);
                  }
                }
              });
              hasNotifySubscribed = true;
              break;
            }
          }
          if (hasNotifySubscribed) break;
        }
      }
    } catch (_) {
      // Failed to discover services
    }
  }
}
