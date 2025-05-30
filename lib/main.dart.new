import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:file_picker/file_picker.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'ESP32 BLE OTA',
      theme: ThemeData(
        primarySwatch: Colors.blue,
        useMaterial3: true,
      ),
      home: const HomePage(),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  List<BluetoothDevice> scanResults = [];
  bool isScanning = false;
  BluetoothDevice? selectedDevice;
  String status = 'Ready';
  double uploadProgress = 0.0;
  StreamSubscription? _scanSubscription;

  final String SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  final String OTA_CHARACTERISTIC = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

  @override
  void initState() {
    super.initState();
    _checkPermissions();
  }

  @override
  void dispose() {
    _scanSubscription?.cancel();
    super.dispose();
  }

  Future<void> _checkPermissions() async {
    if (Platform.isAndroid) {
      await Permission.bluetooth.request();
      await Permission.bluetoothScan.request();
      await Permission.bluetoothConnect.request();
      await Permission.location.request();
    }
  }

  void startScan() async {
    setState(() {
      scanResults.clear();
      isScanning = true;
    });

    try {
      // Start scanning
      await FlutterBluePlus.turnOn();
      
      // Listen for scan results
      _scanSubscription?.cancel();
      _scanSubscription = FlutterBluePlus.onScanResults.listen((results) {
        if (results.isNotEmpty) {
          setState(() {
            scanResults = results.map((r) => r.device).toList();
          });
        }
      }, onError: (e) {
        print('Scan error: $e');
      });

      // Start the scan
      await FlutterBluePlus.startScan(
        timeout: const Duration(seconds: 4),
        androidUsesFineLocation: true,
      );

      // After 4 seconds, stop scanning
      await Future.delayed(const Duration(seconds: 4));
      await FlutterBluePlus.stopScan();
      
      setState(() {
        isScanning = false;
      });
    } catch (e) {
      print('Error during scan: $e');
      setState(() {
        isScanning = false;
        status = 'Scan failed: $e';
      });
    }
  }

  Future<void> connectToDevice(BluetoothDevice device) async {
    setState(() {
      status = 'Connecting...';
      selectedDevice = device;
    });

    try {
      await device.connect(timeout: const Duration(seconds: 4));
      setState(() {
        status = 'Connected';
      });
    } catch (e) {
      setState(() {
        status = 'Connection failed: $e';
      });
    }
  }

  Future<void> uploadFirmware() async {
    if (selectedDevice == null) {
      setState(() {
        status = 'No device selected';
      });
      return;
    }

    try {
      FilePickerResult? result = await FilePicker.platform.pickFiles();
      if (result == null) return;

      File file = File(result.files.single.path!);
      List<int> firmware = await file.readAsBytes();
      
      List<BluetoothService> services = await selectedDevice!.discoverServices();
      BluetoothService otaService = services.firstWhere(
        (s) => s.serviceUuid.toString() == SERVICE_UUID,
      );

      BluetoothCharacteristic otaCharacteristic = otaService.characteristics
          .firstWhere((c) => c.characteristicUuid.toString() == OTA_CHARACTERISTIC);

      // Send firmware size first
      Map<String, dynamic> sizeData = {'size': firmware.length};
      await otaCharacteristic.write(utf8.encode(jsonEncode(sizeData)));

      // Send firmware in chunks
      int chunkSize = 512;
      int sent = 0;

      while (sent < firmware.length) {
        int end = (sent + chunkSize) > firmware.length
            ? firmware.length
            : sent + chunkSize;
        List<int> chunk = firmware.sublist(sent, end);
        await otaCharacteristic.write(chunk, withoutResponse: true);
        sent += chunk.length;

        setState(() {
          uploadProgress = sent / firmware.length;
          status = 'Uploading: ${(uploadProgress * 100).toStringAsFixed(1)}%';
        });
      }

      setState(() {
        status = 'Upload complete';
        uploadProgress = 1.0;
      });
    } catch (e) {
      setState(() {
        status = 'Error: $e';
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('ESP32 BLE OTA'),
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            Text('Status: $status',
                style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 16),
            if (uploadProgress > 0)
              LinearProgressIndicator(value: uploadProgress),
            const SizedBox(height: 16),
            ElevatedButton(
              onPressed: isScanning ? null : startScan,
              child: Text(isScanning ? 'Scanning...' : 'Scan for Devices'),
            ),
            const SizedBox(height: 16),
            Expanded(
              child: ListView.builder(
                itemCount: scanResults.length,
                itemBuilder: (context, index) {
                  BluetoothDevice device = scanResults[index];
                  return ListTile(
                    title: Text(device.platformName.isEmpty
                        ? 'Unknown Device'
                        : device.platformName),
                    subtitle: Text(device.remoteId.toString()),
                    trailing: ElevatedButton(
                      onPressed: () => connectToDevice(device),
                      child: const Text('Connect'),
                    ),
                  );
                },
              ),
            ),
            if (selectedDevice != null)
              ElevatedButton(
                onPressed: uploadFirmware,
                child: const Text('Select and Upload Firmware'),
              ),
          ],
        ),
      ),
    );
  }
} 