#ifndef CONFIGS_H
#define CONFIGS_H

#include <stdint.h>

// ======================================================================
// Firmware Version Information
// ======================================================================
#define FIRMWARE_VERSION "1.0.2"
#define FIRMWARE_NAME "mainPCB"

// ======================================================================
// User Configuration Options
// ======================================================================

// Change this name to the BonicBot code

// Select motor driver type (CYTRON or DDSM)
#define MOTOR_TYPE_CYTRON 0
#define MOTOR_TYPE_DDSM 1
#define MOTOR_TYPE MOTOR_TYPE_DDSM // Change as needed

// Select communication method (BLE, SERIAL or BOTH)
#define COMM_METHOD_BLE 0
#define COMM_METHOD_SERIAL 1
#define COMM_METHOD_BOTH 2
#define COMM_METHOD COMM_METHOD_BLE // Change as needed

// Serial baud rate for main communication (when using SERIAL or BOTH)
#define MAIN_SERIAL_BAUD 115200

// ======================================================================
// Debug mode
// ======================================================================
#define DEBUG true
#define RELAY_PIN 35

// ======================================================================
// Pin Definitions
// ======================================================================
#define LED_PIN 47

// Motor pins
#define MOTOR1_PWM_PIN 4
#define MOTOR1_DIR_PIN 40
#define MOTOR2_PWM_PIN 14
#define MOTOR2_DIR_PIN 39

// Eye board communication
#define HEAD_RXD 9 // RX pin for eye board
#define HEAD_TXD 8 // TX pin for eye board

// Servo communication
#define SERVOS_RXD 17 // RX for all servos
#define SERVOS_TXD 18 // TX for all servos

// BMS communication
#define BMS_RXD 37 // BMS RX
#define BMS_TXD 38 // BMS TX

// DDSM motor communication
#define MOTOR_RX 2 // RX pin for DDSM motor control
#define MOTOR_TX 1 // TX pin for DDSM motor control

// ======================================================================
// Type definitions to match SCServo library
// ======================================================================
// These typedefs replicate the types used in SCServo library
typedef int16_t s16;
typedef uint16_t u16;

// ======================================================================
// Custom Service and Characteristic UUIDs
// ======================================================================
#define CONTROL_SERVICE_UUID "00010000-0000-1000-8000-00805f9b34fb"
#define COMMAND_CHAR_UUID "00010001-0000-1000-8000-00805f9b34fb"

#define FEEDBACK_SERVICE_UUID "00020000-0000-1000-8000-00805f9b34fb"
//===============Characteristics================
#define BATTERY_CHAR_UUID "00020001-0000-1000-8000-00805f9b34fb"
#define DISTANCE_FEEDBACK_CHAR_UUID "00020002-0000-1000-8000-00805f9b34fb"
#define BASE_FEEDBACK_CHAR_UUID "00020003-0000-1000-8000-00805f9b34fb"

#define SERVOS_FEEDBACK_SERVICE_UUID "00030000-0000-1000-8000-00805f9b34fb"
//===============Characteristics===============
#define RIGHT_HAND_SERVOS_FEEDBACK_CHAR_UUID "00030001-0000-1000-8000-00805f9b34fb"
#define LEFT_HAND_SERVOS_FEEDBACK_CHAR_UUID "00030002-0000-1000-8000-00805f9b34fb"
#define HEAD_HAND_SERVOS_FEEDBACK_CHAR_UUID "00030003-0000-1000-8000-00805f9b34fb"
#define SERVO_FEEDBACK_CHAR_UUID "00030004-0000-1000-8000-00805f9b34fb"

// ======================================================================
// Servo Definitions - Updated to match Dart model
// ======================================================================

// Define body part indices for clarity
#define RIGHT_GRIPPER 0
#define RIGHT_WRIST 1
#define RIGHT_ELBOW 2
#define RIGHT_SHOLDER_YAW 3
#define RIGHT_SHOLDER_ROLL 4
#define RIGHT_SHOLDER_PITCH 5
#define LEFT_GRIPPER 6
#define LEFT_WRIST 7
#define LEFT_ELBOW 8
#define LEFT_SHOLDER_YAW 9
#define LEFT_SHOLDER_ROLL 10
#define LEFT_SHOLDER_PITCH 11
#define HEAD_PAN 12
#define HEAD_TILT 13

// Total number of servos
#define TOTAL_SERVOS 14

// Servo name strings for JSON processing
extern const char *SERVO_NAMES[TOTAL_SERVOS];

// Data Type definitions
#define RIGHT_HAND_GROUP "rightHand"
#define LEFT_HAND_GROUP "leftHand"
#define HEAD_GROUP "head"
#define BATTERY "battery"
#define BASE "base"
#define DISTANCE "distance"
#define SERVO "servo"

// ======================================================================
// Global Variables (defined in main.ino, declared as extern here)
// ======================================================================
// Servo position settings
extern byte SERVO_IDS[TOTAL_SERVOS];

// extern s16 SERVO_TARGET[TOTAL_SERVOS];
extern u16 SERVO_SPEED[TOTAL_SERVOS];
extern byte SERVO_ACC[TOTAL_SERVOS];

// Servo group indices
extern byte RIGHT_HAND_INDICES[6];
extern byte LEFT_HAND_INDICES[6];
extern byte HEAD_INDICES[2];

// Flag for servo commands
extern volatile bool servoCommandInProgress;

// Connection state tracking
extern bool deviceConnected;
extern bool oldDeviceConnected;

// Eye mode tracking
extern int currentEyeMode;

extern String BONICBOT_CODE;

#endif // CONFIGS_H