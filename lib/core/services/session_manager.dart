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
  StreamSubscription<String>? _csvSubscription;

  @override
  SessionState build() => SessionState.inactive;

  void setPatient(Patient patient) {
    currentPatient = patient;
  }

  void startSession() {
    if (currentPatient == null) throw Exception("No patient selected");
    sessionId = DateTime.now().millisecondsSinceEpoch.toString();
    startTime = DateTime.now();
    syncCount = 0;
    state = SessionState.active;

    // Listen to CSV stream globally while session is active
    _csvSubscription = ref.read(bleServiceProvider).csvDataStream.listen((chunk) {
      ref.read(csvControllerProvider).addCsvChunk(chunk);
    });

    // Trigger an immediate initial sync so the CSV isn't empty for the first 15 mins
    ref.read(bleServiceProvider).writeCommand(BleCommands.syncData);
    syncCount++;

    final isMock = ref.read(bleServiceProvider).isMock;
    final syncInterval = isMock ? const Duration(seconds: 15) : const Duration(minutes: 15);

    // 15-minute (or 15-second if mock) recurring sync timer
    _syncTimer = Timer.periodic(syncInterval, (timer) {
      if (syncCount < 8 || isMock) { // allow infinite syncs in mock mode for testing
        if (!isMock) syncCount++;
        ref.read(bleServiceProvider).writeCommand(BleCommands.syncData);
      } else {
        stopSession();
      }
    });

    // 2-hour session timer
    _sessionTimer = Timer(const Duration(hours: 2), () {
      stopSession();
    });
  }

  void stopSession() {
    state = SessionState.finished;
    endTime = DateTime.now();
    _syncTimer?.cancel();
    _sessionTimer?.cancel();
    _csvSubscription?.cancel();
    ref.read(bleServiceProvider).writeCommand(BleCommands.stopCollection);
  }
}

final sessionManagerProvider = NotifierProvider<SessionManager, SessionState>(() => SessionManager());
