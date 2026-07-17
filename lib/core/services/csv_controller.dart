import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:csv/csv.dart';
import 'package:share_plus/share_plus.dart';
import 'package:path_provider/path_provider.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

class CsvController extends Notifier<List<List<dynamic>>> {
  bool _headersAdded = false;

  @override
  List<List<dynamic>> build() {
    return [];
  }

  List<List<dynamic>> get rows => state;

  Future<File> getSessionFile(String patientId, String sessionId) async {
    final directory = await getApplicationDocumentsDirectory();
    return File('${directory.path}/patient_${patientId}_session_${sessionId}.csv');
  }

  Future<void> initializeSessionFile(String patientId, String sessionId) async {
    final file = await getSessionFile(patientId, sessionId);
    _headersAdded = false;
    if (await file.exists()) {
      final csvString = await file.readAsString();
      final csvDecoder = Csv(dynamicTyping: true);
      state = csvDecoder.decode(csvString);
      if (state.isNotEmpty) {
        _headersAdded = true;
      }
    } else {
      state = [];
    }
  }

  Future<bool> appendAndVerifyData(String patientId, String sessionId, String rawCsvData) async {
    if (rawCsvData.trim().isEmpty) {
      // Nothing to write, treat as successful sync of 0 rows
      return true;
    }

    final file = await getSessionFile(patientId, sessionId);
    final sizeBefore = await file.exists() ? await file.length() : 0;

    // Parse the new data to filter duplicates
    final csvDecoder = Csv(dynamicTyping: true);
    final incomingRows = csvDecoder.decode(rawCsvData);
    if (incomingRows.isEmpty) return true;

    // Find the last timestamp in the existing file
    final lastTimestamp = await getLastTimestampFromFile(file);

    // Filter incoming rows
    List<List<dynamic>> rowsToAppend = [];
    bool isFirstRowHeader = false;

    // Check if the first row is a header
    final firstRow = incomingRows.first;
    if (firstRow.contains('time') || firstRow.contains('timestamp')) {
      isFirstRowHeader = true;
    }

    // Determine starting index
    int startIndex = isFirstRowHeader ? 1 : 0;

    // If file doesn't exist, we must write the header first
    if (!await file.exists()) {
      if (isFirstRowHeader) {
        rowsToAppend.add(firstRow);
      } else {
        // Fallback header if none provided by hardware
        rowsToAppend.add(['time', 'gsr', 'bodyTemp', 'hr', 'validHR', 'spo2', 'validSPO2', 'bno055_euler_heading', 'bno055_euler_roll', 'bno055_euler_pitch', 'bno055_quat_w', 'bno055_quat_x', 'bno055_quat_y', 'bno055_quat_z', 'bno055_linear_x', 'bno055_linear_y', 'bno055_linear_z', 'bno055_gravity_x', 'bno055_gravity_y', 'bno055_gravity_z', 'bno055_accel_x', 'bno055_accel_y', 'bno055_accel_z', 'bno055_gyro_x', 'bno055_gyro_y', 'bno055_gyro_z', 'bno055_mag_x', 'bno055_mag_y', 'bno055_mag_z', 'bno055_temp', 'bno055_calib_sys', 'bno055_calib_gyro', 'bno055_calib_accel', 'bno055_calib_mag']);
      }
    }

    for (int i = startIndex; i < incomingRows.length; i++) {
      final row = incomingRows[i];
      if (row.isEmpty) continue;
      
      // First column is the timestamp
      final timestamp = int.tryParse(row[0].toString());
      if (timestamp != null) {
        if (lastTimestamp == null || timestamp > lastTimestamp) {
          rowsToAppend.add(row);
        }
      } else {
        // Fallback in case timestamp is not parseable but has data
        rowsToAppend.add(row);
      }
    }

    if (rowsToAppend.isEmpty || (rowsToAppend.length == 1 && !await file.exists() && isFirstRowHeader)) {
      // Nothing new to write
      return true;
    }

    // Encode filtered rows to CSV
    final csvEncoder = Csv();
    final csvString = csvEncoder.encode(rowsToAppend);

    String prefix = "";
    if (await file.exists()) {
      final existingContent = await file.readAsString();
      if (existingContent.isNotEmpty && !existingContent.endsWith('\n') && !existingContent.endsWith('\r')) {
        prefix = "\n";
      }
    }

    // Open file in append mode and write
    final iosMode = await file.exists() ? FileMode.append : FileMode.write;
    await file.writeAsString(prefix + csvString, mode: iosMode, flush: true);

    // Verification: ensure file size increased
    final sizeAfter = await file.length();
    if (sizeAfter <= sizeBefore) {
      return false; // Verification failed
    }

    // Refresh memory rows
    final allContent = await file.readAsString();
    state = csvDecoder.decode(allContent);
    _headersAdded = state.isNotEmpty;

    return true;
  }

  Future<int?> getLastTimestampFromFile(File file) async {
    if (!await file.exists()) return null;
    try {
      final csvDecoder = Csv(dynamicTyping: true);
      final content = await file.readAsString();
      final allRows = csvDecoder.decode(content);
      if (allRows.length <= 1) return null;
      
      // Loop from the bottom up to find the first valid timestamp
      for (int i = allRows.length - 1; i >= 1; i--) {
        final row = allRows[i];
        if (row.isNotEmpty) {
          final ts = int.tryParse(row[0].toString());
          if (ts != null) return ts;
        }
      }
    } catch (_) {}
    return null;
  }

  void addCsvChunk(String chunk) {
    // Legacy support (optional)
    final csvDecoder = Csv(dynamicTyping: true);
    List<List<dynamic>> parsedChunk = csvDecoder.decode(chunk);
    if (parsedChunk.isEmpty) return;

    final updated = List<List<dynamic>>.from(state);
    if (parsedChunk.first.contains('time') || parsedChunk.first.contains('timestamp')) {
      if (!_headersAdded) {
        updated.add(parsedChunk.first);
        _headersAdded = true;
      }
      parsedChunk.removeAt(0);
    }

    updated.addAll(parsedChunk);
    state = updated;
  }

  void clear() {
    state = [];
    _headersAdded = false;
  }

  Future<String> downloadCsv(String patientId, String sessionId) async {
    final file = await getSessionFile(patientId, sessionId);
    if (!await file.exists()) {
      throw Exception("No session CSV file exists to download.");
    }
    
    final xfile = XFile(file.path, name: file.path.split('/').last, mimeType: 'text/csv');
    if (kIsWeb) {
      await xfile.saveTo(xfile.name);
    } else {
      await Share.shareXFiles([xfile], text: 'Downloading CSV...');
    }
    return file.path;
  }

  Future<void> shareCsv(String patientId, String sessionId) async {
    final file = await getSessionFile(patientId, sessionId);
    if (!await file.exists()) {
      throw Exception("No session CSV file exists to share.");
    }

    final xfile = XFile(file.path, name: file.path.split('/').last, mimeType: 'text/csv');
    await Share.shareXFiles(
      [xfile], 
      text: 'Session Data for Patient $patientId'
    );
  }
}
