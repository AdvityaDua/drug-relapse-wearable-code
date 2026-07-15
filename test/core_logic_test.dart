import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:health_wearable/core/models/patient.dart';
import 'package:health_wearable/core/models/wearable_device.dart';
import 'package:health_wearable/core/services/session_manager.dart';
import 'package:health_wearable/core/services/csv_controller.dart';
import 'package:health_wearable/core/services/ble_service.dart';

void main() {
  group('SessionManager Tests', () {
    test('startSession throws if no patient selected', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      
      final manager = container.read(sessionManagerProvider.notifier);
      expect(() => manager.startSession(), throwsException);
    });

    test('startSession sets up correct state', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      
      final manager = container.read(sessionManagerProvider.notifier);
      manager.setPatient(Patient(id: '1', name: 'John'));
      manager.startSession();
      
      expect(container.read(sessionManagerProvider), SessionState.active);
      expect(manager.sessionId, isNotNull);
      expect(manager.startTime, isNotNull);
      expect(manager.syncCount, 1);
      manager.stopSession();
    });
  });

  group('CsvController Tests', () {
    test('parses CSV chunk with headers', () {
      final controller = CsvController();
      final chunk = "timestamp,hr,spO2,temp\n12345,72,98,36.5\n";
      
      controller.addCsvChunk(chunk);
      
      expect(controller.rows.length, 2);
      expect(controller.rows[0], ['timestamp', 'hr', 'spO2', 'temp']);
      expect(controller.rows[1], [12345, 72, 98, 36.5]);
    });

    test('parses multiple chunks correctly', () {
      final controller = CsvController();
      
      final chunk1 = "timestamp,hr,spO2,temp\n12345,72,98,36.5\n";
      final chunk2 = "timestamp,hr,spO2,temp\n12346,73,99,36.6\n";
      
      controller.addCsvChunk(chunk1);
      controller.addCsvChunk(chunk2);
      
      expect(controller.rows.length, 3); // 1 header + 2 data rows
      expect(controller.rows[0], ['timestamp', 'hr', 'spO2', 'temp']);
      expect(controller.rows[1], [12345, 72, 98, 36.5]);
      expect(controller.rows[2], [12346, 73, 99, 36.6]);
    });
  });

  group('BleService Mock Tests', () {
    test('startScan emits mock devices', () async {
      final bleService = BleService()..isMock = true;
      
      final devicesList = <List<WearableDevice>>[];
      final sub = bleService.scannedDevices.listen(devicesList.add);
      
      await bleService.startScan();
      
      await Future.delayed(Duration.zero);
      await sub.cancel();
      
      final hasMockDevice = devicesList.any((list) => 
        list.any((d) => d.id == 'MOCK_ESP32_001' && d.name == 'ESP32 Wearable')
      );
      expect(hasMockDevice, true);
    });

    test('connect updates connection state stream', () async {
      final bleService = BleService()..isMock = true;
      
      final states = <bool>[];
      final sub = bleService.connectionStateStream.listen(states.add);
      
      final connected = await bleService.connect('MOCK_ESP32_001');
      expect(connected, true);
      expect(bleService.isConnected, true);
      
      await bleService.disconnect();
      expect(bleService.isConnected, false);
      
      await Future.delayed(Duration.zero);
      await sub.cancel();
      expect(states, [true, false]);
    });

    test('writeString in mock mode works without exception', () async {
      final bleService = BleService()..isMock = true;
      await bleService.connect('MOCK_ESP32_001');
      
      expect(() => bleService.writeString("HELLO"), returnsNormally);
    });
  });
}
