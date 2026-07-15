import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../core/models/patient.dart';
import '../../core/services/providers.dart';

class PatientManagementScreen extends ConsumerStatefulWidget {
  const PatientManagementScreen({super.key});

  @override
  ConsumerState<PatientManagementScreen> createState() => _PatientManagementScreenState();
}

class _PatientManagementScreenState extends ConsumerState<PatientManagementScreen> {
  final TextEditingController _nameController = TextEditingController();
  final TextEditingController _idController = TextEditingController();

  @override
  void dispose() {
    _nameController.dispose();
    _idController.dispose();
    super.dispose();
  }

  void _addPatient() {
    if (_nameController.text.isNotEmpty) {
      final id = _idController.text.isNotEmpty 
          ? _idController.text 
          : DateTime.now().millisecondsSinceEpoch.toString();
          
      ref.read(patientsListProvider.notifier).addPatient(
        Patient(id: id, name: _nameController.text),
      );
      
      _nameController.clear();
      _idController.clear();
      Navigator.of(context).pop(); // Close the dialog
    }
  }

  void _showAddPatientDialog() {
    showDialog(
      context: context,
      builder: (context) {
        return AlertDialog(
          title: const Text('Add Patient'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(
                controller: _nameController,
                decoration: const InputDecoration(labelText: 'Patient Name', prefixIcon: Icon(Icons.person)),
              ),
              const SizedBox(height: 16),
              TextField(
                controller: _idController,
                decoration: const InputDecoration(labelText: 'Patient ID (Optional)', prefixIcon: Icon(Icons.badge)),
              ),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('Cancel'),
            ),
            ElevatedButton(
              onPressed: _addPatient,
              child: const Text('Add'),
            ),
          ],
        );
      }
    );
  }

  @override
  Widget build(BuildContext context) {
    final patientsList = ref.watch(patientsListProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Select Patient'),
      ),
      body: patientsList.isEmpty
          ? Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(Icons.group_off, size: 80, color: Colors.grey),
                  const SizedBox(height: 16),
                  const Text('No patients available.', style: TextStyle(color: Colors.grey, fontSize: 18)),
                  const SizedBox(height: 16),
                  ElevatedButton.icon(
                    onPressed: _showAddPatientDialog,
                    icon: const Icon(Icons.add),
                    label: const Text('Add Patient'),
                  )
                ],
              ),
            )
          : ListView.separated(
              padding: const EdgeInsets.all(16.0),
              itemCount: patientsList.length,
              separatorBuilder: (context, index) => const Divider(),
              itemBuilder: (context, index) {
                final patient = patientsList[index];
                return ListTile(
                  leading: const CircleAvatar(
                    backgroundColor: Color(0xFFE0F2F1),
                    child: Icon(Icons.person, color: Color(0xFF00BFA5)),
                  ),
                  title: Text(patient.name, style: const TextStyle(fontWeight: FontWeight.bold)),
                  subtitle: Text('ID: ${patient.id}'),
                  trailing: IconButton(
                    icon: const Icon(Icons.delete, color: Colors.red),
                    onPressed: () {
                      ref.read(patientsListProvider.notifier).removePatient(patient.id);
                    },
                  ),
                  onTap: () {
                    // Set as the current patient and return
                    ref.read(patientProvider.notifier).setPatient(patient);
                    if (context.canPop()) {
                      context.pop();
                    } else {
                      context.push('/dashboard');
                    }
                  },
                );
              },
            ),
      floatingActionButton: patientsList.isNotEmpty ? FloatingActionButton(
        onPressed: _showAddPatientDialog,
        child: const Icon(Icons.add),
      ) : null,
    );
  }
}
