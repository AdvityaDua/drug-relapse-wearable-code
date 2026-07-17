import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../core/services/providers.dart';
import '../../core/services/session_manager.dart';
import '../../core/constants/commands.dart';


class DashboardScreen extends ConsumerStatefulWidget {
  const DashboardScreen({super.key});

  @override
  ConsumerState<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends ConsumerState<DashboardScreen> {
  late final TextEditingController _terminalController;

  @override
  void initState() {
    super.initState();
    _terminalController = TextEditingController();
  }

  @override
  void dispose() {
    _terminalController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final patient = ref.watch(patientProvider);
    final bleService = ref.watch(bleServiceProvider);
    final sessionState = ref.watch(sessionManagerProvider);
    final sessionManager = ref.read(sessionManagerProvider.notifier);
    final liveDataString = ref.watch(liveDataProvider).value ?? '';
    final battery = ref.watch(batteryProvider).value;
    final isConnected = ref.watch(connectionStateProvider).value ?? bleService.isConnected;

    String displayVitals = 'No Live Data';
    if (liveDataString.isNotEmpty) {
      try {
        final data = jsonDecode(liveDataString);
        final hr = data['hr'] ?? '--';
        final spo2 = data['spo2'] ?? '--';
        displayVitals = 'HR: $hr bpm, SpO2: $spo2%';
      } catch (e) {
        displayVitals = 'HR: -- bpm, SpO2: --%';
      }
    }

    return Scaffold(
      backgroundColor: const Color(0xFFF5F7FA), // Soft modern background
      appBar: AppBar(
        title: const Text('Dashboard', style: TextStyle(fontWeight: FontWeight.w600)),
        elevation: 0,
        backgroundColor: Colors.transparent,
        foregroundColor: const Color(0xFF1976D2),
        actions: [
          if (battery != null)
            Container(
              margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
              padding: const EdgeInsets.symmetric(horizontal: 12),
              decoration: BoxDecoration(
                color: const Color(0xFFE3F2FD),
                borderRadius: BorderRadius.circular(20),
              ),
              child: Row(
                children: [
                  Icon(Icons.battery_charging_full, color: Colors.green[600], size: 18),
                  const SizedBox(width: 6),
                  Text(
                    '$battery%',
                    style: TextStyle(fontWeight: FontWeight.bold, color: Colors.green[700]),
                  ),
                ],
              ),
            )
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Connection Status Banner
            Container(
              margin: const EdgeInsets.only(bottom: 20),
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
              decoration: BoxDecoration(
                color: isConnected ? Colors.green.shade50 : Colors.red.shade50,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(
                  color: isConnected ? Colors.green.shade300 : Colors.red.shade300,
                ),
              ),
              child: Row(
                children: [
                  Icon(
                    isConnected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
                    color: isConnected ? Colors.green : Colors.red,
                    size: 20,
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: Text(
                      isConnected ? 'Wearable Connected' : 'Wearable Disconnected (Please pair device again)',
                      style: TextStyle(
                        fontSize: 13,
                        fontWeight: FontWeight.bold,
                        color: isConnected ? Colors.green.shade700 : Colors.red.shade700,
                      ),
                    ),
                  ),
                  if (!isConnected)
                    TextButton(
                      onPressed: () => context.push('/pairing'),
                      child: const Text('Connect'),
                    )
                ],
              ),
            ),

            // Patient Card
            Container(
              decoration: BoxDecoration(
                color: Colors.white,
                borderRadius: BorderRadius.circular(16),
                boxShadow: [
                  BoxShadow(color: Colors.black.withOpacity(0.04), blurRadius: 10, offset: const Offset(0, 4)),
                ],
              ),
              padding: const EdgeInsets.all(20),
              child: Row(
                children: [
                  Container(
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: const Color(0xFFE0F2F1),
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: const Icon(Icons.person, color: Color(0xFF00BFA5), size: 32),
                  ),
                  const SizedBox(width: 16),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text('CURRENT PATIENT', style: TextStyle(color: Colors.grey, fontSize: 12, letterSpacing: 1.2, fontWeight: FontWeight.w600)),
                        const SizedBox(height: 4),
                        Text(
                          patient?.name ?? 'No Patient Selected',
                          style: const TextStyle(fontSize: 20, fontWeight: FontWeight.bold, color: Color(0xFF2C3E50)),
                        ),
                      ],
                    ),
                  ),
                  IconButton(
                    icon: const Icon(Icons.edit_note, color: Color(0xFF1976D2), size: 28),
                    onPressed: () => context.push('/patient_management'),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 24),

            // Live Data Card
            Container(
              decoration: BoxDecoration(
                gradient: const LinearGradient(
                  colors: [Color(0xFF1976D2), Color(0xFF0D47A1)],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
                borderRadius: BorderRadius.circular(16),
                boxShadow: [
                  BoxShadow(color: const Color(0xFF1976D2).withOpacity(0.3), blurRadius: 12, offset: const Offset(0, 6)),
                ],
              ),
              padding: const EdgeInsets.all(24.0),
              child: Column(
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: const [
                      Icon(Icons.monitor_heart, color: Colors.white, size: 28),
                      SizedBox(width: 12),
                      Text('Live Sensor Data', style: TextStyle(fontSize: 18, color: Colors.white, fontWeight: FontWeight.w600)),
                    ],
                  ),
                  const SizedBox(height: 20),
                  ElevatedButton(
                    onPressed: () {
                      if (sessionState == SessionState.active) {
                        context.push('/live_data');
                      } else {
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(
                            content: Text('Please start a session first to view live data.'),
                            backgroundColor: Color(0xFFE53935),
                            behavior: SnackBarBehavior.floating,
                          ),
                        );
                      }
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.white,
                      foregroundColor: const Color(0xFF1976D2),
                      elevation: 0,
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(30)),
                      padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 16),
                    ),
                    child: const Text('View Real-time Vitals', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 32),

            // Session Controls
            const Text('SESSION CONTROLS', style: TextStyle(color: Colors.grey, fontSize: 12, letterSpacing: 1.2, fontWeight: FontWeight.w600)),
            const SizedBox(height: 12),
            Row(
              children: [
                Expanded(
                  child: InkWell(
                    onTap: () async {
                      if (!isConnected && !bleService.isMock) {
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(
                            content: Text('Cannot start session: wearable is disconnected.'),
                            backgroundColor: Color(0xFFE53935),
                            behavior: SnackBarBehavior.floating,
                          ),
                        );
                        return;
                      }

                      if (sessionState != SessionState.active) {
                        if (patient == null) {
                          await context.push('/patient_management');
                          final updatedPatient = ref.read(patientProvider);
                          if (updatedPatient != null) {
                            sessionManager.setPatient(updatedPatient);
                            sessionManager.startSession();
                            bleService.writeCommand(BleCommands.startCollection);
                          }
                        } else {
                          sessionManager.setPatient(patient);
                          sessionManager.startSession();
                          bleService.writeCommand(BleCommands.startCollection);
                        }
                      }
                    },
                    child: Container(
                      padding: const EdgeInsets.symmetric(vertical: 20),
                      decoration: BoxDecoration(
                        color: sessionState == SessionState.active ? Colors.grey.shade200 : const Color(0xFF43A047),
                        borderRadius: BorderRadius.circular(16),
                        boxShadow: sessionState == SessionState.active ? [] : [
                          BoxShadow(color: const Color(0xFF43A047).withOpacity(0.3), blurRadius: 10, offset: const Offset(0, 4)),
                        ],
                      ),
                      child: Column(
                        children: [
                          Icon(Icons.play_circle_fill, color: sessionState == SessionState.active ? Colors.grey.shade400 : Colors.white, size: 32),
                          const SizedBox(height: 8),
                          Text('START', style: TextStyle(color: sessionState == SessionState.active ? Colors.grey.shade500 : Colors.white, fontWeight: FontWeight.bold)),
                        ],
                      ),
                    ),
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: InkWell(
                    onTap: () {
                      if (sessionState == SessionState.active) {
                        sessionManager.stopSession();
                      }
                    },
                    child: Container(
                      padding: const EdgeInsets.symmetric(vertical: 20),
                      decoration: BoxDecoration(
                        color: sessionState != SessionState.active ? Colors.grey.shade200 : const Color(0xFFE53935),
                        borderRadius: BorderRadius.circular(16),
                        boxShadow: sessionState != SessionState.active ? [] : [
                          BoxShadow(color: const Color(0xFFE53935).withOpacity(0.3), blurRadius: 10, offset: const Offset(0, 4)),
                        ],
                      ),
                      child: Column(
                        children: [
                          Icon(Icons.stop_circle, color: sessionState != SessionState.active ? Colors.grey.shade400 : Colors.white, size: 32),
                          const SizedBox(height: 8),
                          Text('STOP', style: TextStyle(color: sessionState != SessionState.active ? Colors.grey.shade500 : Colors.white, fontWeight: FontWeight.bold)),
                        ],
                      ),
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 24),
            
            // Actions
            Row(
              children: [
                Expanded(
                  child: OutlinedButton.icon(
                    onPressed: () {
                      if (sessionState == SessionState.active) {
                        bleService.writeCommand(BleCommands.syncData);
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(
                            content: Text('Syncing data from wearable...'),
                            backgroundColor: Color(0xFF1976D2),
                            behavior: SnackBarBehavior.floating,
                          ),
                        );
                      } else {
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(
                            content: Text('Cannot sync without an active session.'),
                            backgroundColor: Color(0xFFE53935),
                            behavior: SnackBarBehavior.floating,
                          ),
                        );
                      }
                    },
                    icon: const Icon(Icons.sync),
                    label: const Text('Manual Sync'),
                    style: OutlinedButton.styleFrom(
                      padding: const EdgeInsets.symmetric(vertical: 16),
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                      side: const BorderSide(color: Color(0xFF1976D2)),
                      foregroundColor: const Color(0xFF1976D2),
                    ),
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: () {
                      context.push('/csv_preview');
                    },
                    icon: const Icon(Icons.table_chart),
                    label: const Text('View CSV'),
                    style: ElevatedButton.styleFrom(
                      padding: const EdgeInsets.symmetric(vertical: 16),
                      backgroundColor: const Color(0xFF2C3E50),
                      foregroundColor: Colors.white,
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                      elevation: 0,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 32),

            // Bluetooth Command Terminal Card
            const Text('STRING COMMAND TERMINAL', style: TextStyle(color: Colors.grey, fontSize: 12, letterSpacing: 1.2, fontWeight: FontWeight.w600)),
            const SizedBox(height: 12),
            Container(
              decoration: BoxDecoration(
                color: Colors.white,
                borderRadius: BorderRadius.circular(16),
                boxShadow: [
                  BoxShadow(color: Colors.black.withOpacity(0.04), blurRadius: 10, offset: const Offset(0, 4)),
                ],
              ),
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  const Text(
                    'Send arbitrary ASCII strings to the device over BLE:',
                    style: TextStyle(color: Colors.grey, fontSize: 13),
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _terminalController,
                          decoration: InputDecoration(
                            hintText: 'Enter command string (e.g. "START")',
                            contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
                            border: OutlineInputBorder(
                              borderRadius: BorderRadius.circular(12),
                              borderSide: const BorderSide(color: Colors.grey),
                            ),
                            focusedBorder: OutlineInputBorder(
                              borderRadius: BorderRadius.circular(12),
                              borderSide: const BorderSide(color: Color(0xFF1976D2)),
                            ),
                          ),
                          onSubmitted: (val) {
                            if (val.isNotEmpty && (isConnected || bleService.isMock)) {
                              bleService.writeString(val);
                              _terminalController.clear();
                              ScaffoldMessenger.of(context).showSnackBar(
                                SnackBar(
                                  content: Text('Sent: "$val"'),
                                  backgroundColor: const Color(0xFF1976D2),
                                  behavior: SnackBarBehavior.floating,
                                ),
                              );
                            }
                          },
                        ),
                      ),
                      const SizedBox(width: 12),
                      ElevatedButton(
                        onPressed: (isConnected || bleService.isMock)
                            ? () {
                                final text = _terminalController.text;
                                if (text.isNotEmpty) {
                                  bleService.writeString(text);
                                  _terminalController.clear();
                                  ScaffoldMessenger.of(context).showSnackBar(
                                    SnackBar(
                                      content: Text('Sent: "$text"'),
                                      backgroundColor: const Color(0xFF1976D2),
                                      behavior: SnackBarBehavior.floating,
                                    ),
                                  );
                                }
                              }
                            : null,
                        style: ElevatedButton.styleFrom(
                          backgroundColor: const Color(0xFF1976D2),
                          foregroundColor: Colors.white,
                          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                          padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
                        ),
                        child: const Text('Send'),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
