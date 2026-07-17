import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:health_wearable/core/models/patient.dart';
import 'package:health_wearable/core/models/wearable_device.dart';
import 'package:health_wearable/core/services/session_manager.dart';
import 'package:health_wearable/core/services/csv_controller.dart';
import 'package:health_wearable/core/services/ble_service.dart';
import 'package:health_wearable/core/services/providers.dart';

import 'dart:io';
import 'package:flutter/services.dart';
import 'package:path_provider_platform_interface/path_provider_platform_interface.dart';

class MockPathProviderPlatform extends PathProviderPlatform {
  @override
  Future<String?> getApplicationDocumentsPath() async {
    final dir = Directory('.dart_tool/test_csv');
    if (!await dir.exists()) {
      await dir.create(recursive: true);
    }
    return dir.path;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  setUpAll(() {
    PathProviderPlatform.instance = MockPathProviderPlatform();
  });

  group('SessionManager Tests', () {
    test('startSession throws if no patient selected', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      
      final manager = container.read(sessionManagerProvider.notifier);
      expect(() => manager.startSession(), throwsException);
    });

    test('startSession sets up correct state', () async {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      
      final manager = container.read(sessionManagerProvider.notifier);
      manager.setPatient(Patient(id: '1', name: 'John'));
      await manager.startSession();
      
      expect(container.read(sessionManagerProvider), SessionState.active);
      expect(manager.sessionId, isNotNull);
      expect(manager.startTime, isNotNull);
      expect(manager.syncCount, 1);
      manager.stopSession();
    });
  });

  group('CsvController Tests', () {
    test('parses CSV chunk with headers', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      final controller = container.read(csvControllerProvider.notifier);
      final chunk = "timestamp,hr,spO2,temp\n12345,72,98,36.5\n";
      
      controller.addCsvChunk(chunk);
      
      expect(controller.rows.length, 2);
      expect(controller.rows[0], ['timestamp', 'hr', 'spO2', 'temp']);
      expect(controller.rows[1], [12345, 72, 98, 36.5]);
    });

    test('parses multiple chunks correctly', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      final controller = container.read(csvControllerProvider.notifier);
      
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

  group('Synchronization, Deduplication, and Lock Tests', () {
    test('CsvController appends data and ignores duplicate timestamps', () async {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      
      final controller = container.read(csvControllerProvider.notifier);
      final patientId = 'p1';
      final sessionId = 's1';
      
      // Clean up file if left from previous runs
      final file = await controller.getSessionFile(patientId, sessionId);
      if (await file.exists()) {
        await file.delete();
      }

      // Initialize/clear the file
      await controller.initializeSessionFile(patientId, sessionId);
      
      // First sync: header and 2 rows
      final chunk1 = "time,hr,spO2\n1000,72,98\n1001,73,99\n";
      final success1 = await controller.appendAndVerifyData(patientId, sessionId, chunk1);
      expect(success1, true);
      expect(controller.rows.length, 3); // 1 header + 2 data rows
      expect(controller.rows[1][0], 1000);
      expect(controller.rows[2][0], 1001);
      
      // Second sync: incoming data contains a duplicate row (1001) and a new row (1002)
      final chunk2 = "time,hr,spO2\n1001,73,99\n1002,74,97\n";
      final success2 = await controller.appendAndVerifyData(patientId, sessionId, chunk2);
      expect(success2, true);
      // It should filter out 1001 because it's <= 1001, and only append 1002
      expect(controller.rows.length, 4); // 1 header + 3 data rows
      expect(controller.rows[3][0], 1002);
      
      // Clean up file after test
      if (await file.exists()) {
        await file.delete();
      }
    });

    test('SessionManager serializes synchronization requests (non-overlapping)', () async {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      final manager = container.read(sessionManagerProvider.notifier);
      final bleService = container.read(bleServiceProvider);
      bleService.isMock = true;

      manager.setPatient(Patient(id: 'p2', name: 'Bob'));
      await manager.startSession();

      // Trigger multiple synchronizations simultaneously
      final sync1 = manager.synchronizeData();
      final sync2 = manager.synchronizeData();

      // Both should complete without throwing
      await expectLater(sync1, completes);
      await expectLater(sync2, completes);

      expect(manager.isSyncInProgress, false);
      
      manager.stopSession();
    });
  });
}
