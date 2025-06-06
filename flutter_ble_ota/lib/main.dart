import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:developer' as developer;
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:file_selector/file_selector.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:http/http.dart' as http;
import 'package:path/path.dart' as path;
import 'package:version/version.dart';

// Constants for GitHub
const String GITHUB_OWNER = "Sonusubi"; // Your GitHub username
const String GITHUB_REPO = "ble_ota";    // Your repo name
const String GITHUB_API_URL = "https://api.github.com/repos/$GITHUB_OWNER/$GITHUB_REPO/releases/latest";
const String GITHUB_API_RELEASES_URL = "https://api.github.com/repos/$GITHUB_OWNER/$GITHUB_REPO/releases";

class Release {
  final String version;
  final String name;
  final String body;
  final String firmwareUrl;
  final DateTime publishedAt;

  Release({
    required this.version,
    required this.name,
    required this.body,
    required this.firmwareUrl,
    required this.publishedAt,
  });

  factory Release.fromJson(Map<String, dynamic> json) {
    String? firmwareUrl;
    List<dynamic> assets = json['assets'];
    
    // First try to find the exact mainPCB.bin file
    for (var asset in assets) {
      if (asset['name'].toString() == 'mainPCB.bin') {
        firmwareUrl = asset['browser_download_url'];
        break;
      }
    }
    
    // If exact match not found, try any .bin file
    if (firmwareUrl == null) {
      for (var asset in assets) {
        if (asset['name'].toString().endsWith('.bin')) {
          firmwareUrl = asset['browser_download_url'];
          break;
        }
      }
    }
    
    // If still no firmware found, throw a more descriptive error
    if (firmwareUrl == null) {
      throw Exception(
        'No firmware file (.bin) found in release ${json['tag_name']}.\n'
        'Available files: ${assets.map((a) => a['name']).join(', ')}'
      );
    }

    return Release(
      version: json['tag_name'].toString().replaceAll('v', ''),
      name: json['name'] ?? 'Release ${json['tag_name']}',
      body: json['body'] ?? 'No description available',
      firmwareUrl: firmwareUrl,
      publishedAt: DateTime.parse(json['published_at']),
    );
  }
}

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
      title: 'BonicBot OTA Update',
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
  final String VERSION_CHARACTERISTIC = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

  String currentVersion = "0.0.0";
  String latestVersion = "0.0.0";
  bool updateAvailable = false;
  String? firmwareUrl;

  StreamSubscription<List<ScanResult>>? _scanSubscription;
  StreamSubscription<BluetoothAdapterState>? _adapterStateSubscription;
  List<Release> availableReleases = [];

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

  Future<void> checkFirmwareVersion() async {
    if (selectedDevice == null) {
      _log('No device selected');
      return;
    }

    try {
      _log('Discovering services');
      List<BluetoothService> services = await selectedDevice!.discoverServices();
      
      BluetoothService otaService = services.firstWhere(
        (s) => s.uuid.toString() == SERVICE_UUID,
      );

      BluetoothCharacteristic versionCharacteristic = otaService.characteristics
          .firstWhere((c) => c.uuid.toString() == VERSION_CHARACTERISTIC);

      List<int> value = await versionCharacteristic.read();
      currentVersion = utf8.decode(value);
      _log('Current firmware version: $currentVersion');

      await checkGitHubRelease();
    } catch (e) {
      _log('Error checking firmware version: $e');
    }
  }

  Future<void> checkGitHubRelease() async {
    try {
      _log('Checking GitHub for latest release');
      final response = await http.get(
        Uri.parse(GITHUB_API_URL),
        headers: {'Accept': 'application/vnd.github.v3+json'},
      );

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        latestVersion = data['tag_name'].toString().replaceAll('v', '');
        _log('Latest version on GitHub: $latestVersion');

        // Find the .bin asset for mainPCB
        List<dynamic> assets = data['assets'];
        for (var asset in assets) {
          if (asset['name'].toString().contains('mainPCB') && 
              asset['name'].toString().endsWith('.bin')) {
            firmwareUrl = asset['browser_download_url'];
            break;
          }
        }

        // Compare versions
        try {
          Version current = Version.parse(currentVersion);
          Version latest = Version.parse(latestVersion);
          updateAvailable = latest > current;

          setState(() {
            status = updateAvailable 
                ? 'Update available: $currentVersion → $latestVersion'
                : 'Firmware is up to date ($currentVersion)';
          });
        } catch (e) {
          _log('Error parsing version numbers: $e');
        }
      } else {
        _log('Failed to check GitHub release: ${response.statusCode}');
      }
    } catch (e) {
      _log('Error checking GitHub release: $e');
    }
  }

  Future<void> showReleasesDialog() async {
    if (selectedDevice == null) {
      _log('No device selected');
      return;
    }

    try {
      _log('Fetching GitHub releases');
      final response = await http.get(
        Uri.parse(GITHUB_API_RELEASES_URL),
        headers: {'Accept': 'application/vnd.github.v3+json'},
      );

      if (response.statusCode == 200) {
        final List<dynamic> data = jsonDecode(response.body);
        setState(() {
          availableReleases = data.map((release) => Release.fromJson(release)).toList();
        });

        if (!mounted) return;

        await showDialog(
          context: context,
          builder: (context) => AlertDialog(
            title: const Text('Available Firmware Versions'),
            content: SizedBox(
              width: double.maxFinite,
              child: ListView.builder(
                shrinkWrap: true,
                itemCount: availableReleases.length,
                itemBuilder: (context, index) {
                  final release = availableReleases[index];
                  return Card(
                    child: ListTile(
                      title: Text('Version ${release.version}'),
                      subtitle: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(release.name),
                          Text(
                            'Released: ${release.publishedAt.toString().split('.')[0]}',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        ],
                      ),
                      trailing: ElevatedButton.icon(
                        onPressed: () async {
                          Navigator.of(context).pop();
                          await downloadAndUploadFirmware(release.firmwareUrl);
                        },
                        icon: const Icon(Icons.system_update),
                        label: const Text('Flash'),
                      ),
                    ),
                  );
                },
              ),
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.of(context).pop(),
                child: const Text('Close'),
              ),
            ],
          ),
        );
      } else {
        _log('Failed to fetch releases: ${response.statusCode}');
      }
    } catch (e) {
      _log('Error fetching releases: $e');
    }
  }

  Future<void> downloadAndUploadFirmware([String? specificUrl]) async {
    final url = specificUrl ?? firmwareUrl;
    if (url == null) {
      _log('No firmware URL available');
      return;
    }

    try {
      _log('Downloading firmware from GitHub');
      final response = await http.get(Uri.parse(url));
      
      if (response.statusCode == 200) {
        List<int> firmware = response.bodyBytes;
        _log('Downloaded firmware: ${firmware.length} bytes');
        
        await performOtaUpdate(firmware);
      } else {
        _log('Failed to download firmware: ${response.statusCode}');
      }
    } catch (e) {
      _log('Error downloading firmware: $e');
    }
  }

  Future<void> performOtaUpdate(List<int> firmware) async {
    if (selectedDevice == null) {
      _log('No device selected');
      return;
    }

    try {
      _log('Discovering services');
      List<BluetoothService> services = await selectedDevice!.discoverServices();
      
      BluetoothService otaService = services.firstWhere(
        (s) => s.uuid.toString() == SERVICE_UUID,
      );

      BluetoothCharacteristic otaCharacteristic = otaService.characteristics
          .firstWhere((c) => c.uuid.toString() == OTA_CHARACTERISTIC);

      // Send firmware size first
      _log('Sending firmware size');
      Map<String, dynamic> sizeData = {'size': firmware.length};
      await otaCharacteristic.write(utf8.encode(jsonEncode(sizeData)));

      // Send firmware in chunks
      int chunkSize = 500;
      int sent = 0;

      while (sent < firmware.length) {
        int end = (sent + chunkSize) > firmware.length
            ? firmware.length
            : sent + chunkSize;
        List<int> chunk = firmware.sublist(sent, end);
        
        await otaCharacteristic.write(chunk, withoutResponse: true);
        await Future.delayed(const Duration(milliseconds: 10));
        
        sent += chunk.length;

        setState(() {
          uploadProgress = sent / firmware.length;
          status = 'Uploading: ${(uploadProgress * 100).toStringAsFixed(1)}%';
        });
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

  Future<void> selectAndUploadFirmware() async {
    if (selectedDevice == null) {
      _log('No device selected');
      return;
    }

    try {
      final XTypeGroup typeGroup = XTypeGroup(
        label: 'Firmware Files',
        extensions: ['bin'],
      );

      final XFile? file = await openFile(acceptedTypeGroups: [typeGroup]);
      if (file == null) {
        _log('No file selected');
        return;
      }

      _log('Reading firmware file: ${path.basename(file.path)}');
      List<int> firmware = await file.readAsBytes();
      _log('Firmware size: ${firmware.length} bytes');

      await performOtaUpdate(firmware);
    } catch (e) {
      _log('Error selecting firmware: $e');
    }
  }

  String _formatDeviceName(BluetoothDevice device) {
    if (device.platformName.isEmpty) {
      // Format MAC address with colons for better readability
      String mac = device.remoteId.toString();
      return 'ESP32 ($mac)';
    }
    return device.platformName;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('BonicBot OTA Update'),
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
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Row(
                  children: [
                    const Icon(Icons.info_outline),
                    const SizedBox(width: 16),
                    Expanded(
                      child: Text(
                        status,
                        style: Theme.of(context).textTheme.titleMedium,
                      ),
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 16),
            if (!_permissionsGranted)
              ElevatedButton.icon(
                onPressed: _checkPermissions,
                icon: const Icon(Icons.security),
                label: const Text('Grant Permissions'),
              ),
            if (_permissionsGranted) ...[
              if (uploadProgress > 0)
                Column(
                  children: [
                    LinearProgressIndicator(value: uploadProgress),
                    const SizedBox(height: 8),
                    Text(
                      '${(uploadProgress * 100).toStringAsFixed(1)}%',
                      style: Theme.of(context).textTheme.bodyLarge,
                    ),
                  ],
                ),
              const SizedBox(height: 16),
              Row(
                children: [
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: isScanning ? null : startScan,
                      icon: const Icon(Icons.bluetooth_searching),
                      label: Text(isScanning ? 'Scanning...' : 'Scan for Devices'),
                    ),
                  ),
                  if (selectedDevice != null) ...[
                    const SizedBox(width: 8),
                    ElevatedButton.icon(
                      onPressed: showReleasesDialog,
                      icon: const Icon(Icons.system_update),
                      label: const Text('Firmware Releases'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Theme.of(context).colorScheme.primaryContainer,
                        foregroundColor: Theme.of(context).colorScheme.onPrimaryContainer,
                      ),
                    ),
                  ],
                ],
              ),
              const SizedBox(height: 16),
              Expanded(
                child: ListView.builder(
                  itemCount: scanResults.length,
                  itemBuilder: (context, index) {
                    ScanResult result = scanResults[index];
                    bool isSelected = selectedDevice?.remoteId == result.device.remoteId;
                    
                    return Card(
                      margin: const EdgeInsets.only(bottom: 8.0),
                      elevation: isSelected ? 4 : 1,
                      child: Padding(
                        padding: const EdgeInsets.all(12.0),
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            ListTile(
                              contentPadding: EdgeInsets.zero,
                              leading: Icon(
                                Icons.bluetooth,
                                color: isSelected ? Theme.of(context).primaryColor : null,
                              ),
                              title: Text(_formatDeviceName(result.device)),
                              subtitle: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text('Signal: ${result.rssi} dBm'),
                                  if (isSelected && currentVersion != "0.0.0")
                                    Text(
                                      'Firmware: $currentVersion',
                                      style: const TextStyle(fontWeight: FontWeight.bold),
                                    ),
                                ],
                              ),
                            ),
                            SizedBox(
                              width: double.infinity,
                              child: Wrap(
                                spacing: 8.0,
                                runSpacing: 8.0,
                                alignment: WrapAlignment.end,
                                children: [
                                  ElevatedButton.icon(
                                    onPressed: () => connectToDevice(result.device),
                                    icon: Icon(isSelected ? Icons.link : Icons.link_outlined),
                                    label: Text(isSelected ? 'Connected' : 'Connect'),
                                  ),
                                  if (isSelected) ...[
                                    const SizedBox(width: 8),
                                    ElevatedButton.icon(
                                      onPressed: checkFirmwareVersion,
                                      icon: const Icon(Icons.system_update),
                                      label: const Text('Check Version'),
                                    ),
                                    if (updateAvailable) ...[
                                      const SizedBox(width: 8),
                                      ElevatedButton.icon(
                                        onPressed: downloadAndUploadFirmware,
                                        icon: const Icon(Icons.download),
                                        label: const Text('Update'),
                                        style: ElevatedButton.styleFrom(
                                          backgroundColor: Theme.of(context).colorScheme.secondary,
                                          foregroundColor: Theme.of(context).colorScheme.onSecondary,
                                        ),
                                      ),
                                    ],
                                  ],
                                ],
                              ),
                            ),
                          ],
                        ),
                      ),
                    );
                  },
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
} 