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

    void resetTimeout() {
      timeoutTimer?.cancel();
      timeoutTimer = Timer(const Duration(milliseconds: 1500), () {
        sub?.cancel();
        if (!completer.isCompleted) {
          completer.complete(buffer.toString());
        }
      });
    }

    try {
      // Setup listener on csvDataStream to accumulate chunked BLE data
      sub = bleService.csvDataStream.listen((chunk) {
        buffer.write(chunk);
        resetTimeout();
      });

      // Start initial timeout timer
      timeoutTimer = Timer(const Duration(seconds: 3), () {
        sub?.cancel();
        if (!completer.isCompleted) {
          completer.complete(buffer.toString());
        }
      });

      // 1. Send the 0x03 (Sync Data) command to the hardware.
      await bleService.writeCommand(BleCommands.syncData);

      // 2. Read only the new data (accumulate it from stream)
      final rawCsvData = await completer.future;

      // 3. Append the new data to the existing CSV file, and
      // 4. Verify that the append operation completed successfully.
      final writeSucceeded = await csvController.appendAndVerifyData(
        currentPatient!.id,
        sessionId!,
        rawCsvData,
      );

      if (writeSucceeded) {
        return true;
      } else {
        throw Exception("CSV write verification failed: File size did not increase.");
      }
    } catch (e) {
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
