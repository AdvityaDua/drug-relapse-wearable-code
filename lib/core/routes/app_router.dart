import 'package:go_router/go_router.dart';
import '../../features/patient/patient_management_screen.dart';
import '../../features/pairing/pairing_screen.dart';
import '../../features/dashboard/dashboard_screen.dart';
import '../../features/csv_preview/csv_preview_screen.dart';
import '../../features/live_data/live_data_screen.dart';

final GoRouter router = GoRouter(
  initialLocation: '/pairing',
  routes: [
    GoRoute(
      path: '/patient_management',
      builder: (context, state) => const PatientManagementScreen(),
    ),
    GoRoute(
      path: '/pairing',
      builder: (context, state) => const PairingScreen(),
    ),
    GoRoute(
      path: '/dashboard',
      builder: (context, state) => const DashboardScreen(),
    ),
    GoRoute(
      path: '/csv_preview',
      builder: (context, state) => const CsvPreviewScreen(),
    ),
    GoRoute(
      path: '/live_data',
      builder: (context, state) => const LiveDataScreen(),
    ),
  ],
);
