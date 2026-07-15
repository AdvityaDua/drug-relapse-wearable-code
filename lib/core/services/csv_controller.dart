import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:csv/csv.dart';
import 'package:share_plus/share_plus.dart';

class CsvController {
  List<List<dynamic>> rows = [];
  bool _headersAdded = false;

  void addCsvChunk(String chunk) {
    final csvDecoder = Csv(dynamicTyping: true);
    List<List<dynamic>> parsedChunk = csvDecoder.decode(chunk);
    
    if (parsedChunk.isEmpty) return;

    if (parsedChunk.first.contains('time') || parsedChunk.first.contains('timestamp')) {
      if (!_headersAdded) {
        rows.add(parsedChunk.first);
        _headersAdded = true;
      }
      parsedChunk.removeAt(0);
    }

    rows.addAll(parsedChunk);
  }

  void clear() {
    rows.clear();
    _headersAdded = false;
  }

  Future<XFile> _createMemoryFile(String patientId, String sessionId) async {
    final csvEncoder = Csv();
    String csvString = csvEncoder.encode(rows);
    final bytes = Uint8List.fromList(utf8.encode(csvString));
    
    final fileName = 'patient_${patientId}_session_$sessionId.csv';
    return XFile.fromData(bytes, name: fileName, mimeType: 'text/csv');
  }

  Future<String> downloadCsv(String patientId, String sessionId) async {
    final xfile = await _createMemoryFile(patientId, sessionId);
    
    if (kIsWeb) {
      // On Web, saveTo directly triggers a native browser download to the user's laptop
      await xfile.saveTo(xfile.name);
    } else {
      // On mobile, it opens the share/save menu
      await Share.shareXFiles([xfile], text: 'Downloading CSV...');
    }
    return xfile.name;
  }

  Future<void> shareCsv(String patientId, String sessionId) async {
    final xfile = await _createMemoryFile(patientId, sessionId);
    await Share.shareXFiles(
      [xfile], 
      text: 'Session Data for Patient $patientId'
    );
  }
}
