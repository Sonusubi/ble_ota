#include "communication.h"

// Global tickers for LED blinking
Ticker ledOnTicker;
Ticker ledOffTicker;

// Data sending interval tickers
Ticker batterySendTicker;
Ticker leftHandServosSendTicker;
Ticker rightHandServosSendTicker;
Ticker headServosSendTicker;
Ticker baseSendTicker;
Ticker distanceSendTicker;
Ticker servoSendTicker;

// Continuous data flags
bool isContinuousBatteryActive = false;
bool isContinuousLeftHandServosActive = false;
bool isContinuousRightHandServosActive = false;
bool isContinuousHeadServosActive = false;
bool isContinuousBaseActive = false;
bool isContinuousDistanceActive = false;
bool isContinousServoActive = false;

// Global BLE objects
BLEServer *pServer = nullptr;
BLEService *controlService = nullptr;
BLEService *feedbackService = nullptr;
BLEService *servosFeedbackService = nullptr;
BLECharacteristic *batteryChar = nullptr;
BLECharacteristic *leftHandServosChar = nullptr;
BLECharacteristic *rightHandServosChar = nullptr;
BLECharacteristic *headServosChar = nullptr;
BLECharacteristic *distanceChar = nullptr;
BLECharacteristic *baseChar = nullptr;
BLECharacteristic *servoChar = nullptr;

// ======================================================================
// BLE Server Callbacks (for connection events)
// ======================================================================
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer) override
  {
    Serial.println("Central connected");
    deviceConnected = true;
    oldDeviceConnected = false; // Force welcome message
    stopBlinking();
    digitalWrite(LED_PIN, HIGH);
    sendDataBasedOnDataType(BATTERY, 3000);
  }

  void onDisconnect(BLEServer *pServer) override
  {
    Serial.println("Central disconnected");
    deviceConnected = false;
    oldDeviceConnected = true; // Force reconnection handling in loop
    // Stop all continuous data sending on disconnection
    stopAllContinuousDataSending();
    pServer->getAdvertising()->start();
    startDisconnectionBlinking();
  }
};

// ======================================================================
// Control Service Callbacks (for receiving commands)
// ======================================================================
class ControlCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    // Convert the incoming value to an Arduino String.
    String rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0)
    {
      processCommand(rxValue);
    }
  }
};

// LED blinking implementation for disconnected state
void startDisconnectionBlinking()
{
  ledOnTicker.attach(1.0, []()
                     { digitalWrite(LED_PIN, HIGH); });
  delay(200);
  ledOffTicker.attach(1.0, []()
                      { digitalWrite(LED_PIN, LOW); });
}

void stopBlinking()
{
  ledOnTicker.detach();
  ledOffTicker.detach();
}

// Initialize BLE services
void initializeBLE()
{
  // Initialize BLE
  BLEDevice::init(BONICBOT_CODE);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // ----- Control Service -----
  controlService = pServer->createService(CONTROL_SERVICE_UUID);
  BLECharacteristic *commandChar = controlService->createCharacteristic(
      COMMAND_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  commandChar->setCallbacks(new ControlCallbacks());
  controlService->start();

  // ----- Feedback Service -----
  feedbackService = pServer->createService(FEEDBACK_SERVICE_UUID);

  // Battery characteristic
  batteryChar = feedbackService->createCharacteristic(
      BATTERY_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  batteryChar->addDescriptor(new BLE2902());

  // Distance sensor characteristic
  distanceChar = feedbackService->createCharacteristic(
      DISTANCE_FEEDBACK_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  distanceChar->addDescriptor(new BLE2902());

  baseChar = feedbackService->createCharacteristic(
      BASE_FEEDBACK_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  baseChar->addDescriptor(new BLE2902());

  feedbackService->start();

  // -----Servos Feedback Service -----
  servosFeedbackService = pServer->createService(SERVOS_FEEDBACK_SERVICE_UUID);

  // Left hand servos characteristic
  leftHandServosChar = servosFeedbackService->createCharacteristic(
      LEFT_HAND_SERVOS_FEEDBACK_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  leftHandServosChar->addDescriptor(new BLE2902());

  // Right hand servos characteristic
  rightHandServosChar = servosFeedbackService->createCharacteristic(
      RIGHT_HAND_SERVOS_FEEDBACK_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  rightHandServosChar->addDescriptor(new BLE2902());

  // Head servos characteristic
  headServosChar = servosFeedbackService->createCharacteristic(
      HEAD_HAND_SERVOS_FEEDBACK_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  headServosChar->addDescriptor(new BLE2902());

  servoChar = servosFeedbackService->createCharacteristic(
      SERVO_FEEDBACK_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  servoChar->addDescriptor(new BLE2902());

  servosFeedbackService->start();

  // Initialize OTA service
  initializeOTAService(pServer);

  // Start advertising all services
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(CONTROL_SERVICE_UUID);
  pAdvertising->addServiceUUID(FEEDBACK_SERVICE_UUID);
  pAdvertising->addServiceUUID(SERVOS_FEEDBACK_SERVICE_UUID);
  pAdvertising->addServiceUUID(OTA_SERVICE_UUID);
  pAdvertising->start();

  if (DEBUG)
    Serial.println("BLE services started and advertising");
}

// Initialize communication systems based on configuration
void initializeCommunication()
{
  // Always initialize Serial for debugging
  Serial.begin(MAIN_SERIAL_BAUD);

#if COMM_METHOD == COMM_METHOD_BLE || COMM_METHOD == COMM_METHOD_BOTH
  // Initialize BLE services
  initializeBLE();

  // Start the core data task if using BLE
  // setupCoreDataTask();
#endif

#if COMM_METHOD == COMM_METHOD_SERIAL || COMM_METHOD == COMM_METHOD_BOTH
  // Serial is already initialized in setup() for debug messages
  if (DEBUG)
    Serial.println("Serial communication ready");
#endif

  // Set initial values on all BLE characteristics if using BLE
#if COMM_METHOD == COMM_METHOD_BLE || COMM_METHOD == COMM_METHOD_BOTH
  DynamicJsonDocument initialDoc(128);
  initialDoc["status"] = "ready";
  initialDoc["version"] = "2.0";
  initialDoc["mode"] = "on-demand";

  String initialJson;
  serializeJson(initialDoc, initialJson);

#endif

  // Start the disconnection blinking for BLE
  startDisconnectionBlinking();
}