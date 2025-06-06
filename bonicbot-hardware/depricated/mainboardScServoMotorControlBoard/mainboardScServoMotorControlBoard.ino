#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Ticker.h>
#include <Wire.h>
#include "Adafruit_VL6180X.h"
#include "CytronMotorDriver.h"
#include <ArduinoJson.h>
#include <SCServo.h>
#include <daly-bms-uart.h>

// ======================================================================
// Debug mode
// ======================================================================
#define DEBUG true

// ======================================================================
// Custom Service and Characteristic UUIDs
// ======================================================================
#define CONTROL_SERVICE_UUID         "00010000-0000-1000-8000-00805f9b34fb"
#define COMMAND_CHAR_UUID            "00010001-0000-1000-8000-00805f9b34fb"

#define FEEDBACK_SERVICE_UUID        "00020000-0000-1000-8000-00805f9b34fb"
#define BATTERY_CHAR_UUID            "00020001-0000-1000-8000-00805f9b34fb"
#define SERVO_FEEDBACK_CHAR_UUID     "00020002-0000-1000-8000-00805f9b34fb"
#define MOTOR_FEEDBACK_CHAR_UUID     "00020003-0000-1000-8000-00805f9b34fb"
#define SENSOR_DATA_CHAR_UUID        "00020004-0000-1000-8000-00805f9b34fb"

// ======================================================================
// LED and Ticker configuration
// ======================================================================
#define LED_PIN 47

Ticker ledOnTicker;
Ticker ledOffTicker;
void startDisconnectionBlinking();  // Forward declaration

// ======================================================================
// Motor Driver Instances
// ======================================================================
CytronMD motor1(PWM_DIR, 8, 8);   // Motor1: PWM on Pin 5, DIR on Pin 41
CytronMD motor2(PWM_DIR, 8, 8);  // Motor2: PWM on Pin 12, DIR on Pin 40

// ======================================================================
// Servo Object Setup
// ======================================================================
SCSCL sc;   // For SC-series servos
SMS_STS st; // For ST-series servos

// ======================================================================
// UART Configuration for Multiple Serial Ports
// ======================================================================
#define RH_RXD 2   // Right hand RX
#define RH_TXD 21  // Right hand TX
#define LH_RXD 10  // Left hand RX
#define LH_TXD 15  // Left hand TX
#define HD_RXD 13  // Head RX
#define HD_TXD 14  // Head TX
#define BMS_RXD 38 // BMS RX
#define BMS_TXD 39 // BMS TX
#define MOTOR_TX 1 // TX pin for Serial1
#define MOTOR_RX 7  // RX pin for Serial1
// Hardware Serial allocation
HardwareSerial SerialBMS(1); // UART1 for BMS
HardwareSerial SerialServo(2);  // UART2 for servos (shared)
HardwareSerial SerialMOTOR(0);

// BMS instance
Daly_BMS_UART bms(SerialBMS);

// Current servo configuration
enum ServoConfig {NONE, RIGHT_HAND, LEFT_HAND, HEAD};
ServoConfig currentConfig = NONE;

// Mutex for serial access
SemaphoreHandle_t serialMutex;

// Flag to track if a servo command is in progress
volatile bool servoCommandInProgress = false;

// ======================================================================
// Servo IDs and Parameters with Safe Limits
// ======================================================================
// Left Hand Servos
byte LH_SC_ID[4] = {3, 4, 5, 6};  // SC servos for left hand
byte LH_ST_ID[2] = {1, 2};  // ST servos for left hand
u16 LH_SC_Target[4] = {570, 630, 380, 380};  // Initial positions
u16 LH_SC_Speed[4] = {0, 0, 0, 0};    // Speed settings
s16 LH_ST_Target[2] = {2030, 2030}; // Initial positions
u16 LH_ST_Speed[2] = {0, 0};    // Speed settings
byte LH_ST_ACC[2] = {50, 50};       // Acceleration

// Safe limits for Left Hand
u16 LH_SC_MIN[4] = {124, 170, 80, 300};  // Min safe positions
u16 LH_SC_MAX[4] = {1022, 630, 680, 600};  // Max safe positions
s16 LH_ST_MIN[2] = {1500, 1900};  // Min safe positions
s16 LH_ST_MAX[2] = {4000, 3600};  // Max safe positions

// Right Hand Servos
byte RH_SC_ID[4] = {3, 4, 5, 6};  // SC servos for right hand
byte RH_ST_ID[2] = {1, 2};  // ST servos for right hand
u16 RH_SC_Target[4] = {454, 440, 400, 400};  // Initial positions
u16 RH_SC_Speed[4] = {0, 0, 0, 0};  // Speed settings
s16 RH_ST_Target[2] = {2030, 2030}; // Initial positions
u16 RH_ST_Speed[2] = {0, 0};    // Speed settings
byte RH_ST_ACC[2] = {50, 50};       // Acceleration

// Safe limits for Right Hand
u16 RH_SC_MIN[4] = {0, 440, 0, 300};  // Min safe positions
u16 RH_SC_MAX[4] = {860, 870, 670, 600};  // Max safe positions
s16 RH_ST_MIN[2] = {100, 490};  // Min safe positions
s16 RH_ST_MAX[2] = {2630, 2100};  // Max safe positions

// Head Servos
byte HD_SC_ID[2] = {1, 2};  // SC servos for head
u16 HD_SC_Target[2] = {1500, 1500};  // Initial positions
u16 HD_SC_Speed[2] = {500, 500};     // Speed settings

// Safe limits for Head
u16 HD_SC_MIN[2] = {300, 300};  // Min safe positions
u16 HD_SC_MAX[2] = {902, 700};  // Max safe positions

// ======================================================================
// Global pointers for feedback characteristics (for updating data)
// ======================================================================
BLECharacteristic *batteryChar;
BLECharacteristic *servoFeedbackChar;
BLECharacteristic *motorFeedbackChar;
BLECharacteristic *sensorDataChar;

// Connection state tracking
bool deviceConnected = false;
bool oldDeviceConnected = false;

// ======================================================================
// Configure Serial for a specific servo group
// ======================================================================
bool configureServoSerial(ServoConfig config) {
  // Skip if already configured for this device
  if (currentConfig == config) return true;
  
  // Take the mutex to ensure exclusive access to the serial port
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("ERROR: Could not get serial mutex for configuration");
    return false;
  }
  
  // End the current serial connection and flush any pending data
  SerialServo.flush();
  SerialServo.end();
  
  // Small delay to ensure proper shutdown
  delay(20);
  
  // Configure for the requested device
  bool success = true;
  switch (config) {
    case RIGHT_HAND:
      SerialServo.begin(1000000, SERIAL_8N1, RH_RXD, RH_TXD);
      currentConfig = RIGHT_HAND;
      if (DEBUG) Serial.println("Configured for Right Hand");
      break;
    case LEFT_HAND:
      SerialServo.begin(1000000, SERIAL_8N1, LH_RXD, LH_TXD);
      currentConfig = LEFT_HAND;
      if (DEBUG) Serial.println("Configured for Left Hand");
      break;
    case HEAD:
      SerialServo.begin(1000000, SERIAL_8N1, HD_RXD, HD_TXD);
      currentConfig = HEAD;
      if (DEBUG) Serial.println("Configured for Head");
      break;
    default:
      // Invalid configuration
      Serial.println("ERROR: Invalid servo configuration requested");
      success = false;
      break;
  }
  
  if (success) {
    // Connect servo controllers to the serial port
    sc.pSerial = &SerialServo;
    st.pSerial = &SerialServo;
    
    // Small delay to stabilize
    delay(50);
  }
  
  // Release the mutex
  xSemaphoreGive(serialMutex);
  return success;
}

// ======================================================================
// Validate and apply servo position limits
// ======================================================================
// Function to enforce safety limits for SC servos
u16 enforceSCLimit(u16 target, u16 min, u16 max) {
  if (target < min) return min;
  if (target > max) return max;
  return target;
}

// Function to enforce safety limits for ST servos
s16 enforceSTLimit(s16 target, s16 min, s16 max) {
  if (target < min) return min;
  if (target > max) return max;
  return target;
}

// ======================================================================
// Update Servo Positions with safety checks
// ======================================================================
bool updateLeftHandServos() {
  if (!configureServoSerial(LEFT_HAND)) {
    Serial.println("ERROR: Failed to configure for Left Hand");
    return false;
  }
  
  // Take the mutex for exclusive serial access
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("ERROR: Could not get serial mutex for Left Hand update");
    return false;
  }
  
  servoCommandInProgress = true;
  
  // Apply safety limits to all target positions
  for (int i = 0; i < 4; i++) {
    LH_SC_Target[i] = enforceSCLimit(LH_SC_Target[i], LH_SC_MIN[i], LH_SC_MAX[i]);
  }
  
  for (int i = 0; i < 2; i++) {
    LH_ST_Target[i] = enforceSTLimit(LH_ST_Target[i], LH_ST_MIN[i], LH_ST_MAX[i]);
  }
  
  // Update SC servos (using synchronous write)
  bool success = true;
  
  // Verify we're in the right configuration
  if (currentConfig != LEFT_HAND) {
    Serial.println("ERROR: Configuration switched during Left Hand update");
    success = false;
  } else {
    // Attempt to update SC servos
    sc.SyncWritePos(LH_SC_ID, 4, LH_SC_Target, 0, LH_SC_Speed);
    
    // Small delay between commands
    delay(20);
    
    // Attempt to update ST servos
    st.SyncWritePosEx(LH_ST_ID, 2, LH_ST_Target, LH_ST_Speed, LH_ST_ACC);
  }
  
  // Ensure all data is sent
  SerialServo.flush();
  
  servoCommandInProgress = false;
  
  // Release the mutex
  xSemaphoreGive(serialMutex);
  
  if (DEBUG) {
    if (success) {
      Serial.println("Left Hand servo update successful");
    } else {
      Serial.println("Left Hand servo update failed");
    }
  }
  
  return success;
}

bool updateRightHandServos() {
  if (!configureServoSerial(RIGHT_HAND)) {
    Serial.println("ERROR: Failed to configure for Right Hand");
    return false;
  }
  
  // Take the mutex for exclusive serial access
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("ERROR: Could not get serial mutex for Right Hand update");
    return false;
  }
  
  servoCommandInProgress = true;
  
  // Apply safety limits to all target positions
  for (int i = 0; i < 4; i++) {
    RH_SC_Target[i] = enforceSCLimit(RH_SC_Target[i], RH_SC_MIN[i], RH_SC_MAX[i]);
  }
  
  for (int i = 0; i < 2; i++) {
    RH_ST_Target[i] = enforceSTLimit(RH_ST_Target[i], RH_ST_MIN[i], RH_ST_MAX[i]);
  }
  
  // Update servos
  bool success = true;
  
  // Verify we're in the right configuration
  if (currentConfig != RIGHT_HAND) {
    Serial.println("ERROR: Configuration switched during Right Hand update");
    success = false;
  } else {
    // Attempt to update SC servos
    sc.SyncWritePos(RH_SC_ID, 4, RH_SC_Target, 0, RH_SC_Speed);
    
    // Small delay between commands
    delay(20);
    
    // Attempt to update ST servos
    st.SyncWritePosEx(RH_ST_ID, 2, RH_ST_Target, RH_ST_Speed, RH_ST_ACC);
  }
  
  // Ensure all data is sent
  SerialServo.flush();
  
  servoCommandInProgress = false;
  
  // Release the mutex
  xSemaphoreGive(serialMutex);
  
  if (DEBUG) {
    if (success) {
      Serial.println("Right Hand servo update successful");
    } else {
      Serial.println("Right Hand servo update failed");
    }
  }
  
  return success;
}

bool updateHeadServos() {
  if (!configureServoSerial(HEAD)) {
    Serial.println("ERROR: Failed to configure for Head");
    return false;
  }
  
  // Take the mutex for exclusive serial access
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("ERROR: Could not get serial mutex for Head update");
    return false;
  }
  
  servoCommandInProgress = true;
  
  // Apply safety limits to all target positions
  for (int i = 0; i < 2; i++) {
    HD_SC_Target[i] = enforceSCLimit(HD_SC_Target[i], HD_SC_MIN[i], HD_SC_MAX[i]);
  }
  
  // Update servos
  bool success = true;
  
  // Verify we're in the right configuration
  if (currentConfig != HEAD) {
    Serial.println("ERROR: Configuration switched during Head update");
    success = false;
  } else {
    // Attempt to update SC servos
    sc.SyncWritePos(HD_SC_ID, 2, HD_SC_Target, 0, HD_SC_Speed);
  }
  
  // Ensure all data is sent
  SerialServo.flush();
  
  servoCommandInProgress = false;
  
  // Release the mutex
  xSemaphoreGive(serialMutex);
  
  if (DEBUG) {
    if (success) {
      Serial.println("Head servo update successful");
    } else {
      Serial.println("Head servo update failed");
    }
  }
  
  return success;
}

// ======================================================================
// BLE Server Callbacks (for connection events)
// ======================================================================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    Serial.println("Central connected");
    deviceConnected = true;
    ledOnTicker.detach();
    ledOffTicker.detach();
    digitalWrite(LED_PIN, HIGH);
  }
  void onDisconnect(BLEServer* pServer) override {
    Serial.println("Central disconnected");
    deviceConnected = false;
    pServer->getAdvertising()->start();
    startDisconnectionBlinking();
  }
};

// ======================================================================
// Control Service Callbacks (for receiving commands)
// ======================================================================
class ControlCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    // Don't process new commands if a servo command is already in progress
    if (servoCommandInProgress) {
      Serial.println("WARNING: Received command while previous servo command in progress, skipping");
      return;
    }
    
    // Convert the incoming value to an Arduino String.
    String rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      Serial.print("Received command: ");
      Serial.println(rxValue);
      
      // Create a JSON document for parsing
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, rxValue);
      if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return;
      }
      int m1,m2;
      // Process motor commands
      if (doc.containsKey("motors")) {
        JsonObject motors = doc["motors"];
        if (motors.containsKey("m1")) {
          m1 = motors["m1"];
          // Apply safety limits to motor speed
          m1 = constrain(m1, -255, 255);
          Serial.print("Setting motor1 speed to: ");
          Serial.println(m1);
          motor1.setSpeed(m1);
        }
        if (motors.containsKey("m2")) {
          m2 = motors["m2"];
          // Apply safety limits to motor speed
          m2 = constrain(m2, -255, 255);
          Serial.print("Setting motor2 speed to: ");
          Serial.println(m2);
          motor2.setSpeed(m2);
        }
          SerialMOTOR.print("[{\"id\":1,\"speed\":");
          SerialMOTOR.print(-m1);
          SerialMOTOR.print(",\"acc_time\":2},{\"id\":2,\"speed\":");
          SerialMOTOR.print(m2);
          SerialMOTOR.print(",\"acc_time\":2}]");
      }
      
      // Process servo commands - with additional validation and safety measures
      if (doc.containsKey("servos")) {
        JsonObject servos = doc["servos"];
        bool updateLeftHand = false;
        bool updateRightHand = false;
        bool updateHead = false;
        
        // Process Left Hand commands
        if (servos.containsKey("leftHand")) {
          JsonObject leftHand = servos["leftHand"];
          bool validCommand = false;
          
          // ST servo targets
          if (leftHand.containsKey("st")) {
            JsonArray stArray = leftHand["st"].as<JsonArray>();
            if (stArray.size() <= 2) {
              int i = 0;
              for (JsonVariant value : stArray) {
                if (i < 2) {
                  s16 targetPos = value.as<int>();
                  // Apply safety limits
                  LH_ST_Target[i] = enforceSTLimit(targetPos, LH_ST_MIN[i], LH_ST_MAX[i]);
                  if (LH_ST_Target[i] != targetPos) {
                    Serial.print("WARNING: Left Hand ST target for ID ");
                    Serial.print(LH_ST_ID[i]);
                    Serial.print(" limited from ");
                    Serial.print(targetPos);
                    Serial.print(" to ");
                    Serial.println(LH_ST_Target[i]);
                  }
                  i++;
                  validCommand = true;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in leftHand.st array");
            }
          }
          
          // SC servo targets
          if (leftHand.containsKey("sc")) {
            JsonArray scArray = leftHand["sc"].as<JsonArray>();
            if (scArray.size() <= 4) {
              int i = 0;
              for (JsonVariant value : scArray) {
                if (i < 4) {
                  u16 targetPos = value.as<int>();
                  // Apply safety limits
                  LH_SC_Target[i] = enforceSCLimit(targetPos, LH_SC_MIN[i], LH_SC_MAX[i]);
                  if (LH_SC_Target[i] != targetPos) {
                    Serial.print("WARNING: Left Hand SC target for ID ");
                    Serial.print(LH_SC_ID[i]);
                    Serial.print(" limited from ");
                    Serial.print(targetPos);
                    Serial.print(" to ");
                    Serial.println(LH_SC_Target[i]);
                  }
                  i++;
                  validCommand = true;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in leftHand.sc array");
            }
          }
          
          // ST servo speeds
          if (leftHand.containsKey("stSpeed")) {
            JsonArray speedArray = leftHand["stSpeed"].as<JsonArray>();
            if (speedArray.size() <= 2) {
              int i = 0;
              for (JsonVariant value : speedArray) {
                if (i < 2) {
                  LH_ST_Speed[i] = constrain(value.as<int>(), 0, 1000);
                  i++;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in leftHand.stSpeed array");
            }
          }
          
          // ST servo acceleration
          if (leftHand.containsKey("stAcc")) {
            JsonArray accArray = leftHand["stAcc"].as<JsonArray>();
            if (accArray.size() <= 2) {
              int i = 0;
              for (JsonVariant value : accArray) {
                if (i < 2) {
                  LH_ST_ACC[i] = constrain(value.as<int>(), 0, 255);
                  i++;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in leftHand.stAcc array");
            }
          }
          
          // SC servo speeds
          if (leftHand.containsKey("scSpeed")) {
            JsonArray speedArray = leftHand["scSpeed"].as<JsonArray>();
            if (speedArray.size() <= 4) {
              int i = 0;
              for (JsonVariant value : speedArray) {
                if (i < 4) {
                  LH_SC_Speed[i] = constrain(value.as<int>(), 0, 1000);
                  i++;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in leftHand.scSpeed array");
            }
          }
          
          updateLeftHand = validCommand;
        }
        
        // Process Right Hand commands
        if (servos.containsKey("rightHand")) {
          JsonObject rightHand = servos["rightHand"];
          bool validCommand = false;
          
          // ST servo targets
          if (rightHand.containsKey("st")) {
            JsonArray stArray = rightHand["st"].as<JsonArray>();
            if (stArray.size() <= 2) {
              int i = 0;
              for (JsonVariant value : stArray) {
                if (i < 2) {
                  s16 targetPos = value.as<int>();
                  // Apply safety limits
                  RH_ST_Target[i] = enforceSTLimit(targetPos, RH_ST_MIN[i], RH_ST_MAX[i]);
                  if (RH_ST_Target[i] != targetPos) {
                    Serial.print("WARNING: Right Hand ST target for ID ");
                    Serial.print(RH_ST_ID[i]);
                    Serial.print(" limited from ");
                    Serial.print(targetPos);
                    Serial.print(" to ");
                    Serial.println(RH_ST_Target[i]);
                  }
                  i++;
                  validCommand = true;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in rightHand.st array");
            }
          }
          
          // SC servo targets
          if (rightHand.containsKey("sc")) {
            JsonArray scArray = rightHand["sc"].as<JsonArray>();
            if (scArray.size() <= 4) {
              int i = 0;
              for (JsonVariant value : scArray) {
                if (i < 4) {
                  u16 targetPos = value.as<int>();
                  // Apply safety limits
                  RH_SC_Target[i] = enforceSCLimit(targetPos, RH_SC_MIN[i], RH_SC_MAX[i]);
                  if (RH_SC_Target[i] != targetPos) {
                    Serial.print("WARNING: Right Hand SC target for ID ");
                    Serial.print(RH_SC_ID[i]);
                    Serial.print(" limited from ");
                    Serial.print(targetPos);
                    Serial.print(" to ");
                    Serial.println(RH_SC_Target[i]);
                  }
                  i++;
                  validCommand = true;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in rightHand.sc array");
            }
          }
          
          // ST servo speeds
          if (rightHand.containsKey("stSpeed")) {
            JsonArray speedArray = rightHand["stSpeed"].as<JsonArray>();
            if (speedArray.size() <= 2) {
              int i = 0;
              for (JsonVariant value : speedArray) {
                if (i < 2) {
                  RH_ST_Speed[i] = constrain(value.as<int>(), 0, 1000);
                  i++;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in rightHand.stSpeed array");
            }
          }
          
          // ST servo acceleration
          if (rightHand.containsKey("stAcc")) {
            JsonArray accArray = rightHand["stAcc"].as<JsonArray>();
            if (accArray.size() <= 2) {
              int i = 0;
              for (JsonVariant value : accArray) {
                if (i < 2) {
                  RH_ST_ACC[i] = constrain(value.as<int>(), 0, 255);
                  i++;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in rightHand.stAcc array");
            }
          }
          
          // SC servo speeds
          if (rightHand.containsKey("scSpeed")) {
            JsonArray speedArray = rightHand["scSpeed"].as<JsonArray>();
            if (speedArray.size() <= 4) {
              int i = 0;
              for (JsonVariant value : speedArray) {
                if (i < 4) {
                  RH_SC_Speed[i] = constrain(value.as<int>(), 0, 1000);
                  i++;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in rightHand.scSpeed array");
            }
          }
          
          updateRightHand = validCommand;
        }
        
        // Process Head commands
        if (servos.containsKey("head")) {
          JsonObject head = servos["head"];
          bool validCommand = false;
          
          // SC servo targets
          if (head.containsKey("sc")) {
            JsonArray scArray = head["sc"].as<JsonArray>();
            if (scArray.size() <= 2) {
              int i = 0;
              for (JsonVariant value : scArray) {
                if (i < 2) {
                  u16 targetPos = value.as<int>();
                  // Apply safety limits
                  HD_SC_Target[i] = enforceSCLimit(targetPos, HD_SC_MIN[i], HD_SC_MAX[i]);
                  if (HD_SC_Target[i] != targetPos) {
                    Serial.print("WARNING: Head SC target for ID ");
                    Serial.print(HD_SC_ID[i]);
                    Serial.print(" limited from ");
                    Serial.print(targetPos);
                    Serial.print(" to ");
                    Serial.println(HD_SC_Target[i]);
                  }
                  i++;
                  validCommand = true;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in head.sc array");
            }
          }
          
          // SC servo speeds
          if (head.containsKey("scSpeed")) {
            JsonArray speedArray = head["scSpeed"].as<JsonArray>();
            if (speedArray.size() <= 2) {
              int i = 0;
              for (JsonVariant value : speedArray) {
                if (i < 2) {
                  HD_SC_Speed[i] = constrain(value.as<int>(), 0, 1000);
                  i++;
                }
              }
            } else {
              Serial.println("ERROR: Too many values in head.scSpeed array");
            }
          }
          
          updateHead = validCommand;
        }
        
        // Update servo positions sequentially with error checking
        if (updateLeftHand) {
          if (!updateLeftHandServos()) {
            Serial.println("ERROR: Failed to update Left Hand servos");
            // Consider sending an error notification to the client here
          }
        }
        
        // Add a small delay between servo group updates to avoid conflicts
        if (updateLeftHand && (updateRightHand || updateHead)) {
          delay(50);
        }
        
        if (updateRightHand) {
          if (!updateRightHandServos()) {
            Serial.println("ERROR: Failed to update Right Hand servos");
            // Consider sending an error notification to the client here
          }
        }
        
        // Add a small delay between servo group updates to avoid conflicts
        if (updateRightHand && updateHead) {
          delay(50);
        }
        
        if (updateHead) {
          if (!updateHeadServos()) {
            Serial.println("ERROR: Failed to update Head servos");
            // Consider sending an error notification to the client here
          }
        }
      }
    }
  }
};

// ======================================================================
// LED Blinking Implementation for Disconnected State
// ======================================================================
void startDisconnectionBlinking() {
  ledOnTicker.attach(1.0, []() {
    digitalWrite(LED_PIN, HIGH);
  });
  delay(200);
  ledOffTicker.attach(1.0, []() {
    digitalWrite(LED_PIN, LOW);
  });
}

// ======================================================================
// BLE Server and Services (global)
// ======================================================================
BLEServer* pServer = nullptr;
BLEService* controlService = nullptr;
BLEService* feedbackService = nullptr;

// ======================================================================
// Hardware Instances
// ======================================================================
Adafruit_VL6180X vl = Adafruit_VL6180X();

// Helper function to create a simple status JSON
void sendStatus(const char* statusMsg) {
  if (!deviceConnected) return;
  
  DynamicJsonDocument statusDoc(128);
  statusDoc["status"] = statusMsg;
  statusDoc["timestamp"] = millis();
  
  String statusJson;
  serializeJson(statusDoc, statusJson);
  
  // Set on all characteristics
  motorFeedbackChar->setValue(statusJson.c_str());
  motorFeedbackChar->notify();
  
  Serial.print("Status sent: ");
  Serial.println(statusMsg);
}

// ======================================================================
// Setup Function
// ======================================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Starting up...");
  
  // Create the mutex for serial port access
  serialMutex = xSemaphoreCreateMutex();
  if (serialMutex == NULL) {
    Serial.println("ERROR: Failed to create serialMutex");
    // Handle this critical error
    while(1) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }

  // Initialize VL6180X sensor
  Wire.begin();
  if (!vl.begin()) {
    Serial.println("Failed to initialize VL6180X sensor");
  } else {
    Serial.println("VL6180X sensor initialized");
  }

  // Initialize BMS Serial
  SerialBMS.begin(9600, SERIAL_8N1, BMS_RXD, BMS_TXD);
  bms.Init();
  Serial.println("BMS initialized");
  SerialMOTOR.begin(115200, SERIAL_8N1, MOTOR_RX, MOTOR_TX);
  Serial.println("MOTOR initialized");
  // Initialize Servo Serial (starting with right hand)
  SerialServo.begin(1000000, SERIAL_8N1, RH_RXD, RH_TXD);
  currentConfig = RIGHT_HAND;
  sc.pSerial = &SerialServo;
  st.pSerial = &SerialServo;
  Serial.println("Servo serial initialized for right hand");

  // Initialize BLE
  BLEDevice::init("BonicBotS1");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // ----- Control Service -----
  controlService = pServer->createService(CONTROL_SERVICE_UUID);
  BLECharacteristic* commandChar = controlService->createCharacteristic(
      COMMAND_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE
  );
  commandChar->setCallbacks(new ControlCallbacks());
  controlService->start();

  // ----- Feedback Service -----
  feedbackService = pServer->createService(FEEDBACK_SERVICE_UUID);
  
  // Battery characteristic with read and notify properties
  batteryChar = feedbackService->createCharacteristic(
      BATTERY_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  batteryChar->addDescriptor(new BLE2902());

  // Servo feedback characteristic with read and notify properties
  servoFeedbackChar = feedbackService->createCharacteristic(
      SERVO_FEEDBACK_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  servoFeedbackChar->addDescriptor(new BLE2902());

  // Motor feedback characteristic with read and notify properties
  motorFeedbackChar = feedbackService->createCharacteristic(
      MOTOR_FEEDBACK_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  motorFeedbackChar->addDescriptor(new BLE2902());

  // Sensor data characteristic with read and notify properties
  sensorDataChar = feedbackService->createCharacteristic(
      SENSOR_DATA_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  sensorDataChar->addDescriptor(new BLE2902());

  feedbackService->start();

  // Start advertising both services
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(CONTROL_SERVICE_UUID);
  pAdvertising->addServiceUUID(FEEDBACK_SERVICE_UUID);
  pAdvertising->start();
  Serial.println("BLE services started and advertising");

  // Initialize motors to stopped state
  motor1.setSpeed(0);
  motor2.setSpeed(0);
  
  // Set initial values on all characteristics
  DynamicJsonDocument initialDoc(128);
  initialDoc["status"] = "ready";
  initialDoc["version"] = "1.0";
  
  String initialJson;
  serializeJson(initialDoc, initialJson);
  
  batteryChar->setValue(initialJson.c_str());
  servoFeedbackChar->setValue(initialJson.c_str());
  motorFeedbackChar->setValue(initialJson.c_str());
  sensorDataChar->setValue(initialJson.c_str());
  
  Serial.println("Setup complete!");
}

// ======================================================================
// Read BMS Data
// ======================================================================
bool readBmsData(JsonObject& battery) {
  bool success = bms.update();
  
  // if (!success) {
  //   battery["error"] = true;
  //   battery["message"] = "BMS update failed";
  //   return false;
  // }
  
  battery["voltage"] = bms.get.packVoltage;
  battery["current"] = bms.get.packCurrent;
  battery["soc"] = bms.get.packSOC;
  battery["temp"] = bms.get.tempAverage;
  
  if (bms.get.packSOC < 0 || bms.get.packSOC > 100) {
    battery["error"] = true;
    battery["message"] = "Invalid SOC data";
    return false;
  }
  
  return true;
}

// ======================================================================
// Read Right Hand Servo Data - Updated JSON Format
// ======================================================================
bool readRightHandServos(JsonObject& rightHand) {
  // Skip if a servo command is in progress
  if (servoCommandInProgress) {
    rightHand["busy"] = true;
    return false;
  }
  
  if (!configureServoSerial(RIGHT_HAND)) {
    rightHand["error"] = "Configuration failed";
    return false;
  }
  
  // Take the mutex for exclusive serial access
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    rightHand["error"] = "Could not get serial mutex";
    return false;
  }
  
  bool success = true;
  
  // Read ST servos - now directly adding to rightHand with s1, s2 naming
  for (int i = 0; i < 2; i++) {
    String servoName = "s" + String(RH_ST_ID[i]);
    JsonObject servo = rightHand.createNestedObject(servoName);
    
    if (st.FeedBack(RH_ST_ID[i]) != -1) {
      servo["pos"] = st.ReadPos(RH_ST_ID[i]);
      servo["speed"] = st.ReadSpeed(RH_ST_ID[i]);
      servo["load"] = st.ReadLoad(RH_ST_ID[i]);
      servo["temp"] = st.ReadTemper(RH_ST_ID[i]);
      servo["type"] = "st"; // Optional type indicator
    } else {
      servo["error"] = true;
      success = false;
    }
  }
  
  // Read SC servos - using s3, s4, s5, s6 naming
  for (int i = 0; i < 4; i++) {
    String servoName = "s" + String(RH_SC_ID[i]);
    JsonObject servo = rightHand.createNestedObject(servoName);
    
    if (sc.FeedBack(RH_SC_ID[i]) != -1) {
      servo["pos"] = sc.ReadPos(RH_SC_ID[i]);
      servo["speed"] = sc.ReadSpeed(RH_SC_ID[i]);
      servo["load"] = sc.ReadLoad(RH_SC_ID[i]);
      servo["temp"] = sc.ReadTemper(RH_SC_ID[i]);
      servo["type"] = "sc"; // Optional type indicator
    } else {
      servo["error"] = true;
      success = false;
    }
  }
  
  // Release the mutex
  xSemaphoreGive(serialMutex);
  
  return success;
}

// ======================================================================
// Read Left Hand Servo Data - Updated JSON Format
// ======================================================================
bool readLeftHandServos(JsonObject& leftHand) {
  // Skip if a servo command is in progress
  if (servoCommandInProgress) {
    leftHand["busy"] = true;
    return false;
  }
  
  if (!configureServoSerial(LEFT_HAND)) {
    leftHand["error"] = "Configuration failed";
    return false;
  }
  
  // Take the mutex for exclusive serial access
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    leftHand["error"] = "Could not get serial mutex";
    return false;
  }
  
  bool success = true;
  
  // Read ST servos
  for (int i = 0; i < 2; i++) {
    String servoName = "s" + String(LH_ST_ID[i]);
    JsonObject servo = leftHand.createNestedObject(servoName);
    
    if (st.FeedBack(LH_ST_ID[i]) != -1) {
      servo["pos"] = st.ReadPos(LH_ST_ID[i]);
      servo["speed"] = st.ReadSpeed(LH_ST_ID[i]);
      servo["load"] = st.ReadLoad(LH_ST_ID[i]);
      servo["temp"] = st.ReadTemper(LH_ST_ID[i]);
      servo["type"] = "st"; // Optional type indicator
    } else {
      servo["error"] = true;
      success = false;
    }
  }
  
  // Read SC servos
  for (int i = 0; i < 4; i++) {
    String servoName = "s" + String(LH_SC_ID[i]);
    JsonObject servo = leftHand.createNestedObject(servoName);
    
    if (sc.FeedBack(LH_SC_ID[i]) != -1) {
      servo["pos"] = sc.ReadPos(LH_SC_ID[i]);
      servo["speed"] = sc.ReadSpeed(LH_SC_ID[i]);
      servo["load"] = sc.ReadLoad(LH_SC_ID[i]);
      servo["temp"] = sc.ReadTemper(LH_SC_ID[i]);
      servo["type"] = "sc"; // Optional type indicator
    } else {
      servo["error"] = true;
      success = false;
    }
  }
  
  // Release the mutex
  xSemaphoreGive(serialMutex);
  
  return success;
}

// ======================================================================
// Read Head Servo Data - Updated JSON Format
// ======================================================================
bool readHeadServos(JsonObject& head) {
  // Skip if a servo command is in progress
  if (servoCommandInProgress) {
    head["busy"] = true;
    return false;
  }
  
  if (!configureServoSerial(HEAD)) {
    head["error"] = "Configuration failed";
    return false;
  }
  
  // Take the mutex for exclusive serial access
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    head["error"] = "Could not get serial mutex";
    return false;
  }
  
  bool success = true;
  
  // Read SC servos
  for (int i = 0; i < 2; i++) {
    String servoName = "s" + String(HD_SC_ID[i]);
    JsonObject servo = head.createNestedObject(servoName);
    
    if (sc.FeedBack(HD_SC_ID[i]) != -1) {
      servo["pos"] = sc.ReadPos(HD_SC_ID[i]);
      servo["speed"] = sc.ReadSpeed(HD_SC_ID[i]);
      servo["load"] = sc.ReadLoad(HD_SC_ID[i]);
      servo["temp"] = sc.ReadTemper(HD_SC_ID[i]);
      servo["type"] = "sc"; // Optional type indicator
    } else {
      servo["error"] = true;
      success = false;
    }
  }
  
  // Release the mutex
  xSemaphoreGive(serialMutex);
  
  return success;
}

// ======================================================================
// Read Sensor Data
// ======================================================================
bool readSensorData(JsonObject& sensors) {
  // Read VL6180X sensor
  uint8_t range = vl.readRange();
  uint8_t status = vl.readRangeStatus();
  
  if (status == VL6180X_ERROR_NONE) {
    sensors["range"] = range;
    sensors["lux"] = vl.readLux(VL6180X_ALS_GAIN_5);
    return true;
  } else {
    sensors["error"] = true;
    sensors["status"] = status;
    return false;
  }
}

// ======================================================================
// Main Loop: Non-blocking periodic updates for feedback data
// ======================================================================
unsigned long lastBmsUpdate = 0;
unsigned long lastServoUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastMotorUpdate = 0;

unsigned long lastStatusUpdate = 0;
unsigned long lastDebugPrint = 0;
unsigned long lastServoConfigChange = 0;

const unsigned long BMS_UPDATE_INTERVAL = 2000;     // 2 seconds
const unsigned long SERVO_UPDATE_INTERVAL = 500;   // 500 milli seconds
const unsigned long MOTOR_UPDATE_INTERVAL = 10000;   // 10 seconds
const unsigned long SENSOR_UPDATE_INTERVAL = 10000;  // 10 seconds

const unsigned long STATUS_UPDATE_INTERVAL = 1000;   // 1 second
const unsigned long DEBUG_PRINT_INTERVAL = 2000;     // 2 seconds
const unsigned long SERVO_CONFIG_INTERVAL = 2000;    // 2 seconds

// For more reliable servo configuration cycling
ServoConfig currentReadConfig = NONE;
int servoReadCounter = 0;

// Simplified loop with minimal servo interaction
void loop() {
  unsigned long now = millis();
  
  // Debug print to confirm loop is running
  if (now - lastDebugPrint >= DEBUG_PRINT_INTERVAL) {
    lastDebugPrint = now;
    Serial.print("Loop running at millis(): ");
    Serial.println(now);
    Serial.print("Connection status: ");
    Serial.println(deviceConnected ? "Connected" : "Disconnected");
  }
  
  // Handling connecting and disconnecting events
  if (deviceConnected && !oldDeviceConnected) {
    // Just connected
    oldDeviceConnected = deviceConnected;
    Serial.println("Device just connected - sending initial data");
    sendStatus("connected");
    // Force immediate update of all data
    lastBmsUpdate = 0;
    lastServoUpdate = 0;
    lastMotorUpdate = 0;
    lastSensorUpdate = 0;
  }
  
  if (!deviceConnected && oldDeviceConnected) {
    // Just disconnected
    oldDeviceConnected = deviceConnected;
    Serial.println("Device just disconnected");
    delay(500); // Give BLE stack time to get ready for new connections
  }
  
  // Only send updates if connected and not during an active servo command
  if (deviceConnected && !servoCommandInProgress) {
    // Battery updates
    if (now - lastBmsUpdate >= BMS_UPDATE_INTERVAL) {
      lastBmsUpdate = now;
      
      // Create JSON document for battery data
      DynamicJsonDocument batteryDoc(256);
      JsonObject battery = batteryDoc.to<JsonObject>();
      
      // Try to read BMS data
      readBmsData(battery);
      
      // Add timestamp
      battery["timestamp"] = now;
      
      // Convert to string and send notification
      String batteryJson;
      serializeJson(batteryDoc, batteryJson);
      batteryChar->setValue(batteryJson.c_str());
      batteryChar->notify();
      
      if (DEBUG) Serial.println("BMS data notified");
    }
    
    // Send status updates
    if (now - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
      lastStatusUpdate = now;
      
      // Create a simple status message
      DynamicJsonDocument statusDoc(200);
      
      // Include basic motor information
      JsonObject motors = statusDoc.createNestedObject("motors");
      motors["m1"] = 0; // Replace with actual motor feedback if available
      motors["m2"] = 0; // Replace with actual motor feedback if available
      
      // Add a timestamp
      statusDoc["timestamp"] = now;
      
      // Convert to string and send notification on motor characteristic
      String statusJson;
      serializeJson(statusDoc, statusJson);
      motorFeedbackChar->setValue(statusJson.c_str());
      motorFeedbackChar->notify();
      
      if (DEBUG) Serial.println("Status updated");
    }
    
    // Update servo data (one servo group at a time on a rotating basis)
    // Only if we're not in the middle of a servo command
    if (now - lastServoUpdate >= SERVO_UPDATE_INTERVAL && !servoCommandInProgress) {
      lastServoUpdate = now;
      
      // Increment counter to rotate through different servo configs
      servoReadCounter = (servoReadCounter + 1) % 3;
      
      // Create JSON document for servo data
      DynamicJsonDocument servoDoc(1024);
      JsonObject servos = servoDoc.createNestedObject("servos");
      
      bool readSuccess = false;
      
      // Read appropriate servo group based on rotating counter
      switch (servoReadCounter) {
        case 0:
          if (DEBUG) Serial.println("Reading Right Hand servos...");
          {
            JsonObject rightHand = servos.createNestedObject("rightHand");
            readSuccess = readRightHandServos(rightHand);
          }
          break;
        case 1:
          if (DEBUG) Serial.println("Reading Left Hand servos...");
          {
            JsonObject leftHand = servos.createNestedObject("leftHand");
            readSuccess = readLeftHandServos(leftHand);
          }
          break;
        case 2:
          if (DEBUG) Serial.println("Reading Head servos...");
          {
            JsonObject head = servos.createNestedObject("head");
            readSuccess = readHeadServos(head);
          }
          break;
      }
      
      // Add timestamp and send notification only if successful
      if (readSuccess) {
        servoDoc["timestamp"] = now;
        
        // Convert to string and send notification
        String servoJson;
        serializeJson(servoDoc, servoJson);
        servoFeedbackChar->setValue(servoJson.c_str());
        servoFeedbackChar->notify();
        
        if (DEBUG) {
          Serial.print("Servo data notified for group: ");
          Serial.println(servoReadCounter);
        }
      } else if (DEBUG) {
        Serial.println("Skipped servo data notification due to error or busy state");
      }
    }
  }
}