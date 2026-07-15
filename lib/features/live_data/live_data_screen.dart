import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../core/services/providers.dart';

class LiveDataScreen extends ConsumerWidget {
  const LiveDataScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final liveDataString = ref.watch(liveDataProvider).value;

    Map<String, dynamic> dataMap = {};
    if (liveDataString != null && liveDataString.isNotEmpty) {
      try {
        dataMap = jsonDecode(liveDataString);
      } catch (e) {
        // Fallback if not valid JSON
        dataMap = {'Raw Data': liveDataString};
      }
    }

    return Scaffold(
      appBar: AppBar(
        title: const Text('Live Sensor Data'),
      ),
      body: dataMap.isEmpty
          ? const Center(child: Text('Waiting for live data...'))
          : Padding(
              padding: const EdgeInsets.all(8.0),
              child: ListView.builder(
                itemCount: dataMap.keys.length,
                itemBuilder: (context, index) {
                  final key = dataMap.keys.elementAt(index);
                  final value = dataMap[key];
                  
                  // Clean up the key for display
                  final displayKey = key
                      .replaceAll('bno055_', 'IMU ')
                      .replaceAll('_', ' ')
                      .toUpperCase();

                  return Card(
                    elevation: 1,
                    margin: const EdgeInsets.symmetric(vertical: 4, horizontal: 8),
                    child: ListTile(
                      title: Text(
                        displayKey,
                        style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 14),
                      ),
                      trailing: Text(
                        value.toString(),
                        style: const TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                          color: Color(0xFF1976D2),
                        ),
                      ),
                    ),
                  );
                },
              ),
            ),
    );
  }
}
