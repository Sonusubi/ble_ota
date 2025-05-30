#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <Update.h>
#include <ArduinoJson.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define OTA_CHARACTERISTIC  "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define FIRMWARE_VERSION    "1.0.0"
#define LED_PIN            2  // Built-in LED

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pOtaCharacteristic = nullptr;
bool deviceConnected = false;
bool updateInProgress = false;
size_t updateSize = 0;
size_t writtenSize = 0;

class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        NimBLEDevice::startAdvertising();
    }
};

class OTACallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        
        if (!updateInProgress) {
            // First packet should be JSON with update size
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, value.c_str());
            
            if (!error) {
                updateSize = doc["size"];
                Serial.printf("Starting update of size: %d\n", updateSize);
                
                if (!Update.begin(updateSize)) {
                    Serial.println("Not enough space to begin OTA");
                    return;
                }
                
                updateInProgress = true;
                writtenSize = 0;
            }
        } else {
            // Write firmware chunk
            size_t chunk_size = value.length();
            Update.write((uint8_t*)value.c_str(), chunk_size);
            writtenSize += chunk_size;
            
            Serial.printf("Written: %d/%d\n", writtenSize, updateSize);
            
            if (writtenSize >= updateSize) {
                if (Update.end(true)) {
                    Serial.println("Update Success!");
                    ESP.restart();
                } else {
                    Serial.println("Update Failed!");
                }
                updateInProgress = false;
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    Serial.printf("Starting BLE OTA Device version %s\n", FIRMWARE_VERSION);

    // Initialize LED
    pinMode(LED_PIN, OUTPUT);

    // Initialize BLE
    NimBLEDevice::init("ESP32_OTA_DEVICE");
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create BLE Service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    
    // Create BLE Characteristic for OTA with both write and write without response
    pOtaCharacteristic = pService->createCharacteristic(
        OTA_CHARACTERISTIC,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
    );
    pOtaCharacteristic->setCallbacks(new OTACallbacks());

    // Start the service
    pService->start();

    // Start advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    
    // Configure advertisement data
    NimBLEAdvertisementData advData;
    advData.setName("ESP32_OTA_DEVICE");
    pAdvertising->setScanResponseData(advData);
    NimBLEDevice::startAdvertising();

    Serial.println("BLE OTA Device Ready!");
}

void loop() {
    // Blink LED to show device is running
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
    delay(1000);
    
    if (deviceConnected) {
        Serial.printf("Connected, Free heap: %d\n", ESP.getFreeHeap());
    }
} 