import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../models/patient.dart';
import 'providers.dart';
import '../constants/commands.dart';

enum SessionState { inactive, active, finished }

class SessionManager extends Notifier<SessionState> {
  Patient? currentPatient;
  String? sessionId;
  DateTime? startTime;
  DateTime? endTime;
  int syncCount = 0;

  Timer? _sessionTimer;
  Timer? _syncTimer;
  
  bool _isSyncing = false;
  bool get isSyncInProgress => _isSyncing;
  Completer<void>? _syncCompleter;

  @override
  SessionState build() => SessionState.inactive;

  void setPatient(Patient patient) {
    currentPatient = patient;
  }

  Future<void> startSession() async {
    if (currentPatient == null) throw Exception("No patient selected");
    sessionId = DateTime.now().millisecondsSinceEpoch.toString();
    startTime = DateTime.now();
    syncCount = 0;
    state = SessionState.active;

    // Initialize/clear the CSV file in CsvController for this session
    await ref.read(csvControllerProvider.notifier).initializeSessionFile(currentPatient!.id, sessionId!);

    // Trigger an immediate initial sync so the CSV isn't empty
    await synchronizeData(silent: true);
    syncCount++;

    final isMock = ref.read(bleServiceProvider).isMock;
    final syncInterval = isMock ? const Duration(seconds: 15) : const Duration(minutes: 15);

    // 15-minute (or 15-second if mock) recurring sync timer
    _syncTimer = Timer.periodic(syncInterval, (timer) async {
      if (syncCount < 8 || isMock) { // allow infinite syncs in mock mode for testing
        if (!isMock) syncCount++;
        try {
          await synchronizeData(silent: true);
        } catch (_) {}
      } else {
        stopSession();
      }
    });

    // 2-hour session timer
    _sessionTimer = Timer(const Duration(hours: 2), () {
      stopSession();
    });
  }

  void stopSession() async {
    state = SessionState.finished;
    endTime = DateTime.now();
    _syncTimer?.cancel();
    _sessionTimer?.cancel();

    final bleService = ref.read(bleServiceProvider);

    try {
      // 1. Send command 0x02 to the hardware
      await bleService.writeCommand(BleCommands.stopCollection);

      // Wait a short moment to let the hardware stop and settle
      await Future.delayed(const Duration(seconds: 1));

      // 2. Perform final synchronization
      await synchronizeData();
    } catch (_) {}
  }

  Future<bool> synchronizeData({bool silent = false}) async {
    if (currentPatient == null || sessionId == null) {
      print("[SYNC] synchronizeData() aborted: patient=${currentPatient?.id}, session=$sessionId");
      return false;
    }

    // Lock verification to prevent overlapping sync operations
    while (_isSyncing) {
      await _syncCompleter?.future;
    }

    _isSyncing = true;
    _syncCompleter = Completer<void>();

    final bleService = ref.read(bleServiceProvider);
    final csvController = ref.read(csvControllerProvider.notifier);

    final completer = Completer<String>();
    final buffer = StringBuffer();
    Timer? timeoutTimer;
    StreamSubscription<String>? sub;
    int chunkCount = 0;

    void finishSync(String reason) {
      if (!completer.isCompleted) {
        // Strip the END_SYNC sentinel before completing
        String data = buffer.toString();
        if (data.contains('END_SYNC')) {
          data = data.replaceAll('END_SYNC\n', '').replaceAll('END_SYNC', '');
        }
        print("[SYNC] Completing sync ($reason). Chunks received: $chunkCount, data length: ${data.length}");
        sub?.cancel();
        completer.complete(data);
      }
    }

    void resetTimeout() {
      timeoutTimer?.cancel();
      timeoutTimer = Timer(const Duration(milliseconds: 1500), () {
        finishSync('inactivity timeout 1.5s');
      });
    }

    try {
      print("[SYNC] Starting synchronizeData() for patient=${currentPatient!.id}, session=$sessionId");

      // Setup listener on csvDataStream to accumulate chunked BLE data
      sub = bleService.csvDataStream.listen((chunk) {
        chunkCount++;
        print("[SYNC] 📦 Chunk #$chunkCount received (${chunk.length} chars)");
        
        // Check for END_SYNC sentinel
        if (chunk.contains('END_SYNC')) {
          // Add only the data before the sentinel
          final sentinelIndex = chunk.indexOf('END_SYNC');
          if (sentinelIndex > 0) {
            buffer.write(chunk.substring(0, sentinelIndex));
          }
          timeoutTimer?.cancel();
          finishSync('END_SYNC sentinel received');
          return;
        }
        
        buffer.write(chunk);
        resetTimeout();
      });

      // Start initial timeout timer (5 seconds to give wearable time to process)
      timeoutTimer = Timer(const Duration(seconds: 5), () {
        finishSync('initial timeout 5s - no data received');
      });

      // 1. Send the 0x03 (Sync Data) command to the hardware.
      print("[SYNC] Sending SYNC_DATA command (0x03)...");
      await bleService.writeCommand(BleCommands.syncData);
      print("[SYNC] SYNC_DATA command sent, waiting for data...");

      // 2. Read only the new data (accumulate it from stream)
      final rawCsvData = await completer.future;

      print("[SYNC] Raw CSV data received: ${rawCsvData.length} chars, ${rawCsvData.isEmpty ? 'EMPTY' : '${rawCsvData.split('\n').length} lines'}");
      if (rawCsvData.isNotEmpty) {
        print("[SYNC] First 200 chars: ${rawCsvData.length > 200 ? rawCsvData.substring(0, 200) : rawCsvData}");
      }

      // 3. Append the new data to the existing CSV file, and
      // 4. Verify that the append operation completed successfully.
      final writeSucceeded = await csvController.appendAndVerifyData(
        currentPatient!.id,
        sessionId!,
        rawCsvData,
      );

      if (writeSucceeded) {
        print("[SYNC] ✅ Sync completed successfully.");
        return true;
      } else {
        print("[SYNC] ⚠️ Sync completed but no new data was received from wearable.");
        return false;
      }
    } catch (e) {
      print("[SYNC] ❌ Sync error: $e");
      rethrow;
    } finally {
      timeoutTimer?.cancel();
      await sub?.cancel();
      _isSyncing = false;
      _syncCompleter?.complete();
    }
  }
}

final sessionManagerProvider = NotifierProvider<SessionManager, SessionState>(() => SessionManager());
