#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include "configs.h"
#include "servo_control.h"
#include "motor_control.h"
#include "sensors.h"
#include "ota_service.h"

// UUID for core data characteristic
// #define CORE_DATA_CHAR_UUID "7bdc81d4-42ce-4934-a2ef-dec40bafb3b2"

// Data sending interval timers
extern Ticker batterySendTicker;
extern Ticker leftHandServosSendTicker;
extern Ticker rightHandServosSendTicker;
extern Ticker headServosSendTicker;
extern Ticker baseSendTicker;
extern Ticker distanceSendTicker;
extern Ticker servoSendTicker;

// Connection status flags
extern bool deviceConnected;
extern bool oldDeviceConnected;

// Continuous data flags
extern bool isContinuousBatteryActive;
extern bool isContinuousLeftHandServosActive;
extern bool isContinuousRightHandServosActive;
extern bool isContinuousHeadServosActive;
extern bool isContinuousBaseActive;
extern bool isContinuousDistanceActive;
extern bool isContinousServoActive;

// External references to BLE objects
extern BLEServer *pServer;
extern BLEService *controlService;
extern BLEService *feedbackService;
extern BLEService *servosFeedbackService;
extern BLECharacteristic *batteryChar;
extern BLECharacteristic *leftHandServosChar;
extern BLECharacteristic *rightHandServosChar;
extern BLECharacteristic *headServosChar;
extern BLECharacteristic *distanceChar;
extern BLECharacteristic *baseChar;
extern BLECharacteristic *servoChar;

// Top-level initialization functions (in communication.cpp)
void initializeCommunication();
void initializeBLE();
void setupCoreDataTask();
void startDisconnectionBlinking();
void stopBlinking();

// Command processing functions (in communication_receive.cpp)
void processCommand(String command);

// Data sending functions (in communication_send.cpp)
void sendResponse(const String &response, const char *characteristicUUID = nullptr);
void sendStatus(const char *statusMsg);
void sendDataBasedOnDataType(const String &dataType, const int intervalMs = 0, const String servoName = "");
void stopContinuousDataSending(const String &dataType);
void stopAllContinuousDataSending();
void processServoCommandsOfGroup(JsonObject servos, String group);

#endif // COMMUNICATION_H