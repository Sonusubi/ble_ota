#include <SCServo.h>
#include "configs.h"
#include "servo_control.h"
#include "motor_control.h"
#include "sensors.h"
#include "communication.h"
#include "ota_service.h"
#include <Preferences.h>

String BONICBOT_CODE = ""; // Default value, can be overwritten from NVS

// ======================================================================
// Global Variables (defined in configs.h as extern)
// ======================================================================
// Servo name strings for JSON processing - Exactly matching names from robot_model.dart
const char *SERVO_NAMES[TOTAL_SERVOS] = {
    "rightGripper",
    "rightWrist",
    "rightElbow",
    "rightSholderYaw",
    "rightSholderRoll",
    "rightSholderPitch",
    "leftGripper",
    "leftWrist",
    "leftElbow",
    "leftSholderYaw",
    "leftSholderRoll",
    "leftSholderPitch",
    "headPan",
    "headTilt",
};

// Servo IDs (1-14)
byte SERVO_IDS[TOTAL_SERVOS] = {
    1, 2, 3, 4, 5, 6,    // Right hand (1-6)
    7, 8, 9, 10, 11, 12, // Left hand (7-12)
    13, 14               // Head (13-14)
};

// // Speed settings (0-1000)
u16 SERVO_SPEED[TOTAL_SERVOS] = {
    0, 0, 0, 0, 0, 0, // Right hand speeds
    0, 0, 0, 0, 0, 0, // Left hand speeds
    0, 0              // Head speeds
};

// // Acceleration settings (0-255)
byte SERVO_ACC[TOTAL_SERVOS] = {
    50, 50, 50, 50, 50, 50, // Right hand acceleration
    50, 50, 50, 50, 50, 50, // Left hand acceleration
    50, 50                  // Head acceleration
};

// Grouping servos for convenient batch control - Updated to use new constants
byte RIGHT_HAND_INDICES[6] = {
    RIGHT_GRIPPER,
    RIGHT_WRIST,
    RIGHT_ELBOW,
    RIGHT_SHOLDER_YAW,
    RIGHT_SHOLDER_ROLL,
    RIGHT_SHOLDER_PITCH,
};

byte LEFT_HAND_INDICES[6] = {
    LEFT_GRIPPER,
    LEFT_WRIST,
    LEFT_ELBOW,
    LEFT_SHOLDER_YAW,
    LEFT_SHOLDER_ROLL,
    LEFT_SHOLDER_PITCH,
};

byte HEAD_INDICES[2] = {
    HEAD_PAN,
    HEAD_TILT,
};

// Flag for servo commands
volatile bool servoCommandInProgress = false;

// Connection state tracking
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Hardware Serial for different peripherals
HardwareSerial SerialMOTOR(0); // UART0 for motors
HardwareSerial SerialBMS(1);   // UART1 for BMS
HardwareSerial SerialServo(2); // UART2 for all servos
// HardwareSerial SerialEye(3);   // UART3 for eye board communication

// Timing variables for loop operations
unsigned long lastDebugPrint = 0;
const unsigned long DEBUG_PRINT_INTERVAL = 2000; // 2 seconds

// ======================================================================
// Serial command processing (for SERIAL or BOTH communication modes)
// ======================================================================
void checkSerialCommands()
{
#if COMM_METHOD == COMM_METHOD_SERIAL || COMM_METHOD == COMM_METHOD_BOTH
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.length() > 0)
    {
      processCommand(command);
    }
  }
#endif
}

// ======================================================================
// Setup Function
// ======================================================================
void setup()
{
  // Setup basic I/O
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);

  // Load BONICBOT_CODE from NVS (if available)
  Preferences prefs;
  prefs.begin("auth", false); // Use namespace "device"
  String code = prefs.getString("key", "");
  prefs.end();
  if (code.length() > 0) {
    BONICBOT_CODE = code;
  }

  // Initialize communication first for debug output
  initializeCommunication();

  Serial.println("Starting BonicBot firmware...");

  // Initialize subsystems
  initializeServos(SerialServo);
  initializeMotors(SerialMOTOR);
  initializeSensors(SerialBMS);

  Serial.println("Setup complete!");
  Serial.print("BONICBOT_CODE: ");
  Serial.println(BONICBOT_CODE);
}

// ======================================================================
// Main Loop: Simplified for on-demand feedback
// ======================================================================
void loop()
{
  unsigned long now = millis();

  // Debug print to confirm loop is running
  if (now - lastDebugPrint >= DEBUG_PRINT_INTERVAL)
  {
    lastDebugPrint = now;
    if (DEBUG)
    {
      Serial.print("Loop running at millis(): ");
      Serial.println(now);
      Serial.print("Connection status: ");
      Serial.println(deviceConnected ? "Connected" : "Disconnected");
    }
  }

  // Check for serial commands (if using serial communication)
  checkSerialCommands();

  // Handling connecting and disconnecting events for BLE
#if COMM_METHOD == COMM_METHOD_BLE || COMM_METHOD == COMM_METHOD_BOTH
  if (deviceConnected && !oldDeviceConnected)
  {
    // Just connected
    oldDeviceConnected = deviceConnected;
    Serial.println("Device just connected - sending welcome status");

    // Send a welcome status message
    sendStatus("connected");
  }

  if (!deviceConnected && oldDeviceConnected)
  {
    // Just disconnected
    oldDeviceConnected = deviceConnected;
    Serial.println("Device just disconnected");
    delay(500); // Give BLE stack time to get ready for new connections
  }
#endif

  // The rest of the processing happens in the callback functions when commands are received
}