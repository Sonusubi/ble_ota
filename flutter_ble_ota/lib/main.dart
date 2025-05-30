import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:developer' as developer;
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:file_selector/file_selector.dart';
import 'package:permission_handler/permission_handler.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  FlutterBluePlus.setLogLevel(LogLevel.verbose, color: true);
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
  List<ScanResult> scanResults = [];
  bool isScanning = false;
  BluetoothDevice? selectedDevice;
  String status = 'Ready';
  double uploadProgress = 0.0;
  bool _permissionsGranted = false;

  final String SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  final String OTA_CHARACTERISTIC = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

  StreamSubscription<List<ScanResult>>? _scanSubscription;
  StreamSubscription<BluetoothAdapterState>? _adapterStateSubscription;

  void _log(String message) {
    developer.log(message, name: 'BLE_OTA');
    setState(() {
      status = message;
    });
  }

  @override
  void initState() {
    super.initState();
    _log('App initialized');
    _initBleAndPermissions();
  }

  Future<void> _initBleAndPermissions() async {
    _log('Initializing BLE and checking permissions');
    await _checkPermissions();
    
    try {
      _adapterStateSubscription = FlutterBluePlus.adapterState.listen(
        (state) {
          _log('Bluetooth state changed: $state');
          if (state == BluetoothAdapterState.on) {
            setState(() {
              status = 'Bluetooth is on';
            });
          } else {
            setState(() {
              scanResults.clear();
              selectedDevice = null;
              status = 'Bluetooth is off';
            });
          }
        },
        onError: (error) {
          _log('Bluetooth adapter error: $error');
        },
      );

      final state = await FlutterBluePlus.adapterState.first;
      _log('Initial Bluetooth state: $state');
    } catch (e) {
      _log('Error initializing BLE: $e');
    }
  }

  @override
  void dispose() {
    _log('Disposing resources');
    _scanSubscription?.cancel();
    _adapterStateSubscription?.cancel();
    super.dispose();
  }

  Future<void> _checkPermissions() async {
    _log('Checking permissions');
    if (!Platform.isAndroid) {
      _log('Not Android platform, skipping permissions');
      return;
    }

    try {
      // Check current permission status first
      Map<Permission, PermissionStatus> currentStatus = {
        Permission.bluetooth: await Permission.bluetooth.status,
        Permission.bluetoothScan: await Permission.bluetoothScan.status,
        Permission.bluetoothConnect: await Permission.bluetoothConnect.status,
        Permission.bluetoothAdvertise: await Permission.bluetoothAdvertise.status,
        Permission.location: await Permission.location.status,
      };

      // Show rationale for permissions that need to be requested
      List<Permission> permissionsToRequest = [];
      currentStatus.forEach((permission, status) {
        if (!status.isGranted) {
          permissionsToRequest.add(permission);
        }
      });

      if (permissionsToRequest.isNotEmpty) {
        bool shouldRequest = await showDialog(
          context: context,
          barrierDismissible: false,
          builder: (context) => AlertDialog(
            title: const Text('Permissions Required'),
            content: SingleChildScrollView(
              child: ListBody(
                children: [
                  const Text('This app needs the following permissions to work with Bluetooth devices:'),
                  const SizedBox(height: 10),
                  ...permissionsToRequest.map((permission) => Padding(
                    padding: const EdgeInsets.symmetric(vertical: 4),
                    child: Text('• ${_getPermissionDescription(permission)}'),
                  )),
                ],
              ),
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(context, false),
                child: const Text('Deny'),
              ),
              TextButton(
                onPressed: () => Navigator.pop(context, true),
                child: const Text('Allow'),
              ),
            ],
          ),
        ) ?? false;

        if (!shouldRequest) {
          _log('User denied permission request dialog');
          setState(() {
            _permissionsGranted = false;
            status = 'Permissions denied by user';
          });
          return;
        }
      }

      // Request permissions
      Map<Permission, PermissionStatus> statuses = await Map.fromEntries(
        await Future.wait(
          permissionsToRequest.map((permission) async {
            final status = await permission.request();
            return MapEntry(permission, status);
          }),
        ),
      );

      // Merge with existing granted permissions
      statuses.addAll(Map.fromEntries(
        currentStatus.entries.where((entry) => entry.value.isGranted),
      ));

      // Check results
      bool allGranted = true;
      List<String> deniedPermissions = [];

      statuses.forEach((permission, status) {
        _log('Permission ${permission.toString()}: ${status.toString()}');
        if (!status.isGranted) {
          allGranted = false;
          deniedPermissions.add(_getPermissionDescription(permission));
        }
      });

      setState(() {
        _permissionsGranted = allGranted;
        if (!allGranted) {
          status = 'Missing permissions:\n${deniedPermissions.join("\n")}';
        } else {
          status = 'All permissions granted';
        }
      });

      // If any permission is permanently denied, show settings dialog
      if (statuses.values.any((status) => status.isPermanentlyDenied)) {
        _log('Some permissions are permanently denied');
        final shouldOpenSettings = await showDialog<bool>(
          context: context,
          builder: (context) => AlertDialog(
            title: const Text('Permissions Required'),
            content: const Text(
              'Some permissions are permanently denied. Please enable them in app settings to use BLE features.',
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(context, false),
                child: const Text('Cancel'),
              ),
              TextButton(
                onPressed: () => Navigator.pop(context, true),
                child: const Text('Open Settings'),
              ),
            ],
          ),
        ) ?? false;

        if (shouldOpenSettings) {
          await openAppSettings();
        }
      }
    } catch (e) {
      _log('Error checking permissions: $e');
      setState(() {
        status = 'Error checking permissions: $e';
        _permissionsGranted = false;
      });
    }
  }

  String _getPermissionDescription(Permission permission) {
    switch (permission) {
      case Permission.bluetooth:
        return 'Bluetooth: Required for basic Bluetooth functionality';
      case Permission.bluetoothScan:
        return 'Bluetooth Scan: Required to find nearby devices';
      case Permission.bluetoothConnect:
        return 'Bluetooth Connect: Required to connect to devices';
      case Permission.bluetoothAdvertise:
        return 'Bluetooth Advertise: Required for BLE advertising';
      case Permission.location:
        return 'Location: Required for Bluetooth scanning on Android';
      default:
        return permission.toString();
    }
  }

  void startScan() async {
    if (isScanning) {
      _log('Already scanning');
      return;
    }

    if (!_permissionsGranted) {
      _log('Permissions not granted, requesting permissions');
      await _checkPermissions();
      if (!_permissionsGranted) {
        setState(() {
          status = 'Cannot scan: permissions not granted';
        });
        return;
      }
    }

    try {
      // Check if Bluetooth is on
      final adapterState = await FlutterBluePlus.adapterState.first;
      _log('Current Bluetooth state: $adapterState');
      
      if (adapterState != BluetoothAdapterState.on) {
        _log('Bluetooth is not on');
        setState(() {
          status = 'Please turn on Bluetooth';
        });
        return;
      }

      setState(() {
        scanResults.clear();
        isScanning = true;
        status = 'Scanning...';
      });

      await FlutterBluePlus.startScan(
        timeout: const Duration(seconds: 4),
        androidUsesFineLocation: true,
      );

      _scanSubscription = FlutterBluePlus.scanResults.listen(
        (results) {
          _log('Found ${results.length} devices');
          setState(() {
            scanResults = results;
          });
        },
        onError: (e) {
          _log('Scan error: $e');
          setState(() {
            status = 'Scan error: $e';
            isScanning = false;
          });
        },
      );

      await Future.delayed(const Duration(seconds: 4));
      await FlutterBluePlus.stopScan();
      _log('Scan completed');
    } catch (e) {
      _log('Error during scan: $e');
      setState(() {
        status = 'Error starting scan: $e';
      });
    } finally {
      setState(() {
        isScanning = false;
      });
    }
  }

  Future<void> connectToDevice(BluetoothDevice device) async {
    _log('Connecting to device: ${device.platformName}');
    setState(() {
      status = 'Connecting...';
      selectedDevice = device;
    });

    try {
      await device.connect(timeout: const Duration(seconds: 4));
      _log('Connected successfully');
      setState(() {
        status = 'Connected';
      });
    } catch (e) {
      _log('Connection failed: $e');
      setState(() {
        status = 'Connection failed: $e';
        selectedDevice = null;
      });
    }
  }

  Future<void> uploadFirmware() async {
    if (selectedDevice == null) {
      _log('No device selected');
      setState(() {
        status = 'No device selected';
      });
      return;
    }

    try {
      _log('Opening file picker');
      final XTypeGroup typeGroup = XTypeGroup(
        label: 'Binary files',
        extensions: ['bin'],
      );

      final XFile? file = await openFile(acceptedTypeGroups: [typeGroup]);
      if (file == null) {
        _log('No file selected');
        return;
      }

      _log('Reading file: ${file.path}');
      List<int> firmware = await file.readAsBytes();
      _log('File size: ${firmware.length} bytes');
      
      _log('Discovering services');
      List<BluetoothService> services = await selectedDevice!.discoverServices();
      _log('Found ${services.length} services');

      _log('Looking for OTA service');
      BluetoothService otaService = services.firstWhere(
        (s) => s.uuid.toString() == SERVICE_UUID,
        orElse: () {
          throw Exception('OTA service not found');
        },
      );
      _log('Found OTA service');

      _log('Looking for OTA characteristic');
      BluetoothCharacteristic otaCharacteristic = otaService.characteristics
          .firstWhere(
            (c) => c.uuid.toString() == OTA_CHARACTERISTIC,
            orElse: () {
              throw Exception('OTA characteristic not found');
            },
          );
      _log('Found OTA characteristic');

      // Send firmware size first
      _log('Sending firmware size');
      Map<String, dynamic> sizeData = {'size': firmware.length};
      await otaCharacteristic.write(utf8.encode(jsonEncode(sizeData)));

      // Send firmware in chunks
      int chunkSize = 244;
      int sent = 0;

      while (sent < firmware.length) {
        int end = (sent + chunkSize) > firmware.length
            ? firmware.length
            : sent + chunkSize;
        List<int> chunk = firmware.sublist(sent, end);
        
        // Add delay between chunks to prevent buffer overflow
        await otaCharacteristic.write(chunk, withoutResponse: true);
        await Future.delayed(const Duration(milliseconds: 10));
        
        sent += chunk.length;

        setState(() {
          uploadProgress = sent / firmware.length;
          status = 'Uploading: ${(uploadProgress * 100).toStringAsFixed(1)}%';
        });
        _log('Uploaded ${(uploadProgress * 100).toStringAsFixed(1)}%');
      }

      _log('Upload complete');
      setState(() {
        status = 'Upload complete';
        uploadProgress = 1.0;
      });
    } catch (e) {
      _log('Error during upload: $e');
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
        actions: [
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: _checkPermissions,
            tooltip: 'Check Permissions',
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            Text('Status: $status',
                style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 16),
            if (!_permissionsGranted)
              ElevatedButton(
                onPressed: _checkPermissions,
                child: const Text('Grant Permissions'),
              ),
            if (_permissionsGranted) ...[
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
                    ScanResult result = scanResults[index];
                    return ListTile(
                      title: Text(result.device.platformName.isEmpty
                          ? 'Unknown Device'
                          : result.device.platformName),
                      subtitle: Text(result.device.remoteId.toString()),
                      trailing: ElevatedButton(
                        onPressed: () => connectToDevice(result.device),
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
          ],
        ),
      ),
    );
  }
} 