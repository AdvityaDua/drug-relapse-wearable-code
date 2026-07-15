/// A simple model to represent a discovered BLE device,
/// used by both the mock and the real BLE implementations.
class WearableDevice {
  final String id;
  final String name;
  final int rssi;

  WearableDevice({
    required this.id,
    required this.name,
    required this.rssi,
  });
}
