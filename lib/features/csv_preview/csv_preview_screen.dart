import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:pluto_grid/pluto_grid.dart';
import '../../core/services/providers.dart';

class CsvPreviewScreen extends ConsumerStatefulWidget {
  const CsvPreviewScreen({super.key});

  @override
  ConsumerState<CsvPreviewScreen> createState() => _CsvPreviewScreenState();
}

class _CsvPreviewScreenState extends ConsumerState<CsvPreviewScreen> {
  List<PlutoColumn> columns = [];
  List<PlutoRow> rows = [];

  @override
  void initState() {
    super.initState();
  }

  void _buildGridData() {
    columns.clear();
    rows.clear();
    final csvController = ref.read(csvControllerProvider);
    final csvRows = csvController.rows;

    if (csvRows.isEmpty) return;

    // First row is headers
    final headers = csvRows.first;
    for (var header in headers) {
      columns.add(
        PlutoColumn(
          title: header.toString(),
          field: header.toString(),
          type: PlutoColumnType.text(),
        ),
      );
    }

    // Remaining rows are data
    for (int i = 1; i < csvRows.length; i++) {
      final rowData = csvRows[i];
      Map<String, PlutoCell> cells = {};
      for (int j = 0; j < headers.length; j++) {
        cells[headers[j].toString()] = PlutoCell(value: j < rowData.length ? rowData[j].toString() : '');
      }
      rows.add(PlutoRow(cells: cells));
    }
  }

  @override
  Widget build(BuildContext context) {
    // Keep watching csvChunks to trigger rebuild if new data comes in
    ref.watch(csvChunksProvider);
    final csvController = ref.watch(csvControllerProvider);
    final patient = ref.watch(patientProvider);
    final sessionManager = ref.read(sessionManagerProvider.notifier);

    // If new data arrived, we rebuild the grid
    _buildGridData();

    return Scaffold(
      appBar: AppBar(
        title: const Text('CSV Preview'),
        actions: [
          IconButton(
            icon: const Icon(Icons.download),
            tooltip: 'Download CSV',
            onPressed: () async {
              if (patient != null) {
                final path = await csvController.downloadCsv(patient.id, sessionManager.sessionId ?? 'unknown');
                if (context.mounted) {
                  ScaffoldMessenger.of(context).showSnackBar(SnackBar(
                    content: Text('Saved to: $path'),
                    duration: const Duration(seconds: 3),
                    backgroundColor: const Color(0xFF388E3C),
                  ));
                }
              } else {
                ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('No patient selected')));
              }
            },
          ),
          IconButton(
            icon: const Icon(Icons.share),
            tooltip: 'Share CSV',
            onPressed: () {
              if (patient != null) {
                csvController.shareCsv(patient.id, sessionManager.sessionId ?? 'unknown');
              } else {
                ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('No patient selected')));
              }
            },
          ),
          const SizedBox(width: 8),
        ],
      ),
      body: csvController.rows.isEmpty
          ? const Center(child: Text('No CSV data available. Sync data from the device.'))
          : Container(
              margin: const EdgeInsets.all(16.0),
              decoration: BoxDecoration(
                color: Colors.white,
                borderRadius: BorderRadius.circular(12),
                boxShadow: [
                  BoxShadow(
                    color: Colors.black.withOpacity(0.05),
                    blurRadius: 10,
                    offset: const Offset(0, 4),
                  ),
                ],
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(12),
                child: PlutoGrid(
                  key: ValueKey(csvController.rows.length),
                  columns: columns,
                  rows: rows,
                  configuration: const PlutoGridConfiguration(
                    style: PlutoGridStyleConfig(
                      enableCellBorderVertical: false,
                      gridBorderColor: Colors.transparent,
                      gridBackgroundColor: Colors.white,
                      rowColor: Colors.white,
                      oddRowColor: Color(0xFFF5F7FA), // Soft off-white for alternating rows
                      activatedColor: Color(0xFFE3F2FD),
                      gridBorderRadius: BorderRadius.all(Radius.circular(12)),
                      columnTextStyle: TextStyle(
                        color: Color(0xFF1976D2), // Medical Blue
                        fontWeight: FontWeight.bold,
                        fontSize: 14,
                      ),
                      cellTextStyle: TextStyle(
                        color: Colors.black87,
                        fontSize: 13,
                      ),
                      iconColor: Color(0xFF1976D2),
                    ),
                  ),
                ),
              ),
            ),
    );
  }
}
