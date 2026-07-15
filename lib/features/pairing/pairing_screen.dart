import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../core/services/providers.dart';
import '../../core/models/wearable_device.dart';

class PairingScreen extends ConsumerStatefulWidget {
  const PairingScreen({super.key});

  @override
  ConsumerState<PairingScreen> createState() => _PairingScreenState();
}

class _PairingScreenState extends ConsumerState<PairingScreen> {
  bool _isConnecting = false;
  String? _connectingToId;

  @override
  Widget build(BuildContext context) {
    final bleService = ref.watch(bleServiceProvider);
    final isScanning = ref.watch(isScanningProvider).value ?? false;
    final isConnected = ref.watch(connectionStateProvider).value ?? bleService.isConnected;
    final battery = ref.watch(batteryProvider).value ?? bleService.batteryLevel;
    final devices = ref.watch(scannedDevicesProvider).value ?? [];

    final isMockMode = ref.watch(mockModeProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Pair Device'),
        actions: [
          Row(
            children: [
              const Text('Mock', style: TextStyle(fontSize: 12)),
              Switch(
                value: isMockMode,
                onChanged: (value) {
                  ref.read(mockModeProvider.notifier).setMode(value);
                  bleService.isMock = value;
                  if (isScanning) {
                    bleService.stopScan();
                  }
                },
              ),
            ],
          ),
          if (battery != null)
            Padding(
              padding: const EdgeInsets.only(right: 16.0),
              child: Center(
                child: Row(
                  children: [
                    const Icon(Icons.battery_full, size: 18),
                    const SizedBox(width: 4),
                    Text(
                      'Wearable Battery: $battery%',
                      style: const TextStyle(fontWeight: FontWeight.bold),
                    ),
                  ],
                ),
              ),
            ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            // ── Connection status banner ──────────────────────────────
            AnimatedContainer(
              duration: const Duration(milliseconds: 300),
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              decoration: BoxDecoration(
                color: isConnected
                    ? Colors.green.shade50
                    : Colors.grey.shade100,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(
                  color: isConnected
                      ? Colors.green.shade300
                      : Colors.grey.shade300,
                ),
              ),
              child: Row(
                children: [
                  Icon(
                    isConnected
                        ? Icons.bluetooth_connected
                        : Icons.bluetooth_disabled,
                    color: isConnected
                        ? Colors.green
                        : Colors.grey,
                    size: 28,
                  ),
                  const SizedBox(width: 12),
                  Text(
                    isConnected
                        ? 'Connected to Wearable'
                        : 'Not Connected',
                    style: TextStyle(
                      fontSize: 16,
                      fontWeight: FontWeight.bold,
                      color: isConnected
                          ? Colors.green.shade700
                          : Colors.grey.shade700,
                    ),
                  ),
                ],
              ),
            ),

            const SizedBox(height: 20),

            // ── Scan / Stop buttons ───────────────────────────────────
            Row(
              children: [
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: isScanning || isConnected
                        ? null
                        : () async {
                            try {
                              await bleService.startScan();
                            } catch (e) {
                              if (context.mounted) {
                                ScaffoldMessenger.of(context).showSnackBar(
                                  SnackBar(
                                    content: Text('Error starting scan: ${e.toString().replaceAll('PlatformException(startScan, ', '').replaceAll(', null, null)', '').replaceAll(')', '')}'),
                                    backgroundColor: Colors.red,
                                  ),
                                );
                              }
                            }
                          },
                    icon: isScanning
                        ? const SizedBox(
                            width: 18,
                            height: 18,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Icon(Icons.search),
                    label: Text(isScanning ? 'Scanning…' : 'Scan for Devices'),
                  ),
                ),
                if (isScanning) ...[
                  const SizedBox(width: 12),
                  OutlinedButton(
                    onPressed: () => bleService.stopScan(),
                    child: const Text('Stop'),
                  ),
                ],
                if (isConnected) ...[
                  const SizedBox(width: 12),
                  OutlinedButton.icon(
                    onPressed: () async {
                      await bleService.disconnect();
                      setState(() {});
                    },
                    icon: const Icon(Icons.bluetooth_disabled),
                    label: const Text('Disconnect'),
                  ),
                ],
              ],
            ),

            const SizedBox(height: 16),

            // ── Device list ───────────────────────────────────────────
            if (!isConnected) ...[
              if (devices.isEmpty && !isScanning)
                const Expanded(
                  child: Center(
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(Icons.bluetooth_searching,
                            size: 64, color: Colors.grey),
                        SizedBox(height: 12),
                        Text(
                          'No devices found.\nPress "Scan for Devices" to start.',
                          textAlign: TextAlign.center,
                          style: TextStyle(color: Colors.grey),
                        ),
                      ],
                    ),
                  ),
                )
              else
                Expanded(
                  child: ListView.separated(
                    itemCount: devices.length,
                    separatorBuilder: (_, _i) => const Divider(height: 1),
                    itemBuilder: (context, index) {
                      final device = devices[index];
                      final isThisConnecting =
                          _isConnecting && _connectingToId == device.id;
                      return _DeviceTile(
                        device: device,
                        isConnecting: isThisConnecting,
                        onTap: () => _connectToDevice(device),
                      );
                    },
                  ),
                ),
            ] else ...[
              // Connected – show Go to Dashboard button
              const Spacer(),
              SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  onPressed: () => context.push('/dashboard'),
                  icon: const Icon(Icons.dashboard),
                  label: const Text('Go to Dashboard'),
                  style: ElevatedButton.styleFrom(
                    padding: const EdgeInsets.symmetric(vertical: 16),
                  ),
                ),
              ),
              const SizedBox(height: 32),
            ],
          ],
        ),
      ),
    );
  }

  Future<void> _connectToDevice(WearableDevice device) async {
    setState(() {
      _isConnecting = true;
      _connectingToId = device.id;
    });

    final bleService = ref.read(bleServiceProvider);
    final success = await bleService.connect(device.id);

    if (mounted) {
      setState(() {
        _isConnecting = false;
        _connectingToId = null;
      });

      if (!success) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Failed to connect to ${device.name}'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }
}

// ── Device tile widget ───────────────────────────────────────────────────────
class _DeviceTile extends StatelessWidget {
  final WearableDevice device;
  final bool isConnecting;
  final VoidCallback onTap;

  const _DeviceTile({
    required this.device,
    required this.isConnecting,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    // Signal strength icon based on RSSI
    IconData signalIcon;
    Color signalColor;
    if (device.rssi > -60) {
      signalIcon = Icons.signal_cellular_alt;
      signalColor = Colors.green;
    } else if (device.rssi > -80) {
      signalIcon = Icons.signal_cellular_alt_2_bar;
      signalColor = Colors.orange;
    } else {
      signalIcon = Icons.signal_cellular_alt_1_bar;
      signalColor = Colors.red;
    }

    return ListTile(
      leading: const CircleAvatar(
        backgroundColor: Color(0xFFE0F2F1),
        child: Icon(Icons.watch, color: Color(0xFF00BFA5)),
      ),
      title: Text(
        device.name,
        style: const TextStyle(fontWeight: FontWeight.bold),
      ),
      subtitle: Text('ID: ${device.id}'),
      trailing: isConnecting
          ? const SizedBox(
              width: 24,
              height: 24,
              child: CircularProgressIndicator(strokeWidth: 2),
            )
          : Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(signalIcon, color: signalColor, size: 18),
                const SizedBox(width: 4),
                Text(
                  '${device.rssi} dBm',
                  style:
                      TextStyle(fontSize: 12, color: Colors.grey.shade600),
                ),
              ],
            ),
      onTap: isConnecting ? null : onTap,
    );
  }
}
