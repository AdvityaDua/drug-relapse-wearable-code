import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:health_wearable/main.dart';

void main() {
  testWidgets('App boots and shows pairing screen', (WidgetTester tester) async {
    // Build our app and trigger a frame.
    await tester.pumpWidget(const ProviderScope(child: MyApp()));

    // Verify that the initial screen is the Pairing Screen
    expect(find.text('Pair Device'), findsOneWidget);
  });
}
