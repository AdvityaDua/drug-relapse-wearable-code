class BleUuids {
  // ── ESP32 Custom Wearable Service (Nordic UART Service) ──
  /// The primary service UUID that the wearable advertises.
  static const String espServiceUuid = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
  
  /// The characteristic the phone uses to WRITE commands to the wearable (TX from Phone -> RX on ESP32).
  static const String espRxCharacteristicUuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; 
  
  /// The characteristic the phone uses to READ/LISTEN to data from the wearable (RX on Phone <- TX from ESP32).
  static const String espTxCharacteristicUuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; 

  // ── Standard BLE Services ──
  /// Standard BLE Battery Service UUID (0x180F)
  static const String batteryServiceUuid = "0000180f-0000-1000-8000-00805f9b34fb";
  
  /// Standard BLE Battery Level Characteristic UUID (0x2A19)
  static const String batteryLevelCharacteristicUuid = "00002a19-0000-1000-8000-00805f9b34fb";
}
