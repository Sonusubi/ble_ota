# ESP32 BLE OTA Update System

This project consists of two parts:
1. ESP32 firmware that enables OTA updates via BLE
2. Flutter app to send firmware updates to the ESP32

## ESP32 Firmware Setup

### Prerequisites
- PlatformIO IDE (recommended to install it as a VSCode extension)
- ESP32 development board

### Building and Flashing
1. Open the `esp32_ota_ble` directory in PlatformIO
2. Connect your ESP32 board
3. Build and upload the firmware:
   ```bash
   cd esp32_ota_ble
   pio run -t upload
   ```
4. Monitor the serial output (optional):
   ```bash
   pio device monitor
   ```

## Flutter App Setup

### Prerequisites
- Flutter SDK
- Android Studio or Xcode (for mobile development)
- Android device or iPhone for testing

### Building and Running
1. Install dependencies:
   ```bash
   cd flutter_ble_ota
   flutter pub get
   ```
2. Run the app:
   ```bash
   flutter run
   ```

## Usage

1. Flash the initial firmware to your ESP32 board
2. Launch the Flutter app on your mobile device
3. Click "Scan for Devices" to find your ESP32
4. Connect to your ESP32 (it will appear as "ESP32_OTA_DEVICE")
5. Click "Select and Upload Firmware" to choose a new firmware file
6. Wait for the upload to complete

## Notes

- The ESP32 will automatically restart after a successful update
- Make sure Bluetooth is enabled on your mobile device
- For Android, location permissions are required for BLE scanning
- The firmware is sent in chunks of 512 bytes to ensure reliable transmission

## Troubleshooting

1. If the device is not found:
   - Make sure Bluetooth is enabled
   - Check if location services are enabled (Android)
   - Restart the ESP32

2. If the update fails:
   - Make sure the ESP32 is powered properly
   - Try reducing the distance between the phone and ESP32
   - Check the serial monitor for detailed error messages 