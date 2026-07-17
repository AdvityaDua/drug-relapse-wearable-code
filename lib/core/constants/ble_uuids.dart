class BleUuids {
  // ── ESP32 Custom Wearable Service ──
  /// The primary service UUID that the wearable advertises.
  static const String espServiceUuid = "8A6F0001-5C2E-4C89-8C67-3E6E52E10001";
  
  /// The characteristic the phone uses to WRITE commands to the wearable (TX from Phone -> RX on ESP32).
  static const String espRxCharacteristicUuid = "8A6F0002-5C2E-4C89-8C67-3E6E52E10002"; 
  
  /// The characteristic the phone uses to READ/LISTEN to data from the wearable (RX on Phone <- TX from ESP32).
  static const String espTxCharacteristicUuid = "8A6F0004-5C2E-4C89-8C67-3E6E52E10004";

  // ── Standard BLE Services ──
  /// Standard BLE Battery Service UUID (0x180F)
  static const String batteryServiceUuid = "0000180f-0000-1000-8000-00805f9b34fb";
  
  /// Standard BLE Battery Level Characteristic UUID (0x2A19)
  static const String batteryLevelCharacteristicUuid = "00002a19-0000-1000-8000-00805f9b34fb";
}
