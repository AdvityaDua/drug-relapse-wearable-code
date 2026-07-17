import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'ble_service.dart';
import 'session_manager.dart';
export 'session_manager.dart';
import 'csv_controller.dart';
import '../constants/commands.dart';
import '../models/patient.dart';
import '../models/wearable_device.dart';

// BleService Provider
final bleServiceProvider = Provider<BleService>((ref) {
  return BleService();
});

// CSV Controller Provider
final csvControllerProvider = NotifierProvider<CsvController, List<List<dynamic>>>(() {
  return CsvController();
});

// SessionManager is now a Notifier and defines its own provider in session_manager.dart

// Patient Provider State (Current Selected Patient)
class PatientNotifier extends Notifier<Patient?> {
  @override
  Patient? build() => null;

  void setPatient(Patient patient) {
    state = patient;
  }
}

final patientProvider = NotifierProvider<PatientNotifier, Patient?>(() => PatientNotifier());

// Patients List State (All Patients)
class PatientsListNotifier extends Notifier<List<Patient>> {
  @override
  List<Patient> build() => [];

  void addPatient(Patient patient) {
    state = [...state, patient];
  }

  void removePatient(String id) {
    state = state.where((p) => p.id != id).toList();
  }
}

final patientsListProvider = NotifierProvider<PatientsListNotifier, List<Patient>>(() => PatientsListNotifier());

// Mock Mode State
class MockModeNotifier extends Notifier<bool> {
  @override
  bool build() => false;

  void toggle() {
    state = !state;
  }
  
  void setMode(bool isMock) {
    state = isMock;
  }
}

final mockModeProvider = NotifierProvider<MockModeNotifier, bool>(() => MockModeNotifier());

// Streams from BleService
final isScanningProvider = StreamProvider<bool>((ref) {
  return ref.watch(bleServiceProvider).isScanning;
});

// Connection state provider
final connectionStateProvider = StreamProvider<bool>((ref) {
  return ref.watch(bleServiceProvider).connectionStateStream;
});

// Scanned devices list
final scannedDevicesProvider = StreamProvider<List<WearableDevice>>((ref) {
  return ref.watch(bleServiceProvider).scannedDevices;
});

// Battery – uses keepAlive so the last emitted value persists across rebuilds
final batteryProvider = StreamProvider<int>((ref) {
  ref.keepAlive();
  return ref.watch(bleServiceProvider).batteryStream;
});

final liveDataProvider = StreamProvider<String>((ref) {
  return ref.watch(bleServiceProvider).liveDataStream;
});


