# DalyBMS Test Software

This folder contains software and scripts for the Daly Battery Management System (BMS). The software is designed to interface with the Daly BMS hardware and perform testing and and changing parameters of battery like changing capacity and calibration.

## Features
- Communication with Daly BMS via UART.
- Test scripts for validating BMS functionality.
- Software for updation of BMS parameters like capacity and calibration.
## Usage
-To use the DalyBMS test software, ensure that the main PCB is connected to bms via bms port in the main PCB. The software can be run on a computer connected to the main PCB via USB.
-Upload the usb to uart program in the test folder to main pcb `tests/dallyBMS_test/usbtouart/usbtouart.ino`
-run `tests/dallyBMS_test/DalyBmsMonitorV1.1.4/PCMaster.exe` on the computer connected to the main PCB via USB.
-Select the correct com port in the DalyBMS Monitor software.
-Select the correct baud rate(9600) in the DalyBMS Monitor software.
-From parameter setting in DalyBMS Monitor software, provide required parameters like capacity and cell voltage.
-Set cumilative charge and discharge  to 0 in DalyBMS Monitor software initially.
-For checking the parameter updation, go to readParam in DalyBMS Monitor software and read the required parameters.
## Requirements
- Main PCB with USB-to-UART capability.
- Daly BMS hardware.
- Properly configured UART communication settings.