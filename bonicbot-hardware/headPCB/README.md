# Bonic Bot Head PCB

This project is designed to control the head PCB of the Bonic Bot. It includes functionality for rendering eye animations on TFT displays and interfacing with the TF-Luna distance sensor.

---

## Features

- **TFT Display Control**: Displays animations and images on TFT screens.
- **TF-Luna Distance Sensor**: Reads distance data and provides it via serial communication.
- **Modes**: Supports multiple operational modes for different animations.

---

## Hardware Requirements

- **ESP32**: Microcontroller used for controlling the system.
- **TFT Display**: For rendering eye animations.
- **TF-Luna Sensor**: For distance measurement.

---

## Software Requirements

- **Arduino IDE**: For compiling and uploading the code.
- **TFT_eSPI Library**: For controlling the TFT display.

---

## Setup

1. Clone this repository.
2. Install the required libraries in the Arduino IDE:
   - [TFT_eSPI] from the library folder in the repo: `bonicbot-hardware/libraries/TFT_eSPI`.


---

## Usage

1. Upload the code to the ESP32 using the Arduino IDE.
2. For testing use `tests/uart_communication/uart_communication.ino` code as main board code
3. Send the following commands to control the system from main board:
   - `1`: Activates the first demo mode.
   - `2`: Activates the second demo mode.
   - `3`: Activates the third demo mode.
   - `4`: Activates the fourth demo mode.
   - `d`: Retrieves the current distance from the TF-Luna sensor.
4. Use I2C-4 port from main board for UART communication
---

## File Structure

- **`headPCB.ino`**: Main program file.
- **`config.h`**: Configuration file for hardware settings.
- **`EYEA.h`**: Header file for eye animations.

---

## Pinout Information

### **0.71-inch LCD Module**
| Pin | Connection |
|-----|------------|
| VCC | 5V         |
| GND | GND        |
| DIN | GPIO7      |
| CLK | GPIO6      |
| CS  | GPIO5      |
| DC  | GPIO4      |
| RST | GPIO8      |
| BL  | 3V3        |

### **TF-Luna Sensor**
| Pin | Connection |
|-----|------------|
| TX  | GPIO12     |
| RX  | GPIO13     |

### **Serial Communication with Main Board**
| Pin | Connection |
|-----|------------|
| TX  | GPIO11     |
| RX  | GPIO10     |

---

## Board Version

- **ESP32 Board Version**: `2.0.12`

---

## Project Configuration (Arduino Tools)

- **Board**: ESP32S3 Dev Module
- **USB CDC on Boot**: Enabled
- **Flash Size**: 4MB (32Mb)
- **Partition Scheme**: Huge App (3MB no OTA / 1MB SPIFFS)

---

## Notes

- Ensure all connections are made according to the pinout structure provided above.