#include "ota_service.h"
#include "configs.h"

BLEService* otaService = nullptr;
BLECharacteristic* otaCharacteristic = nullptr;
BLECharacteristic* versionCharacteristic = nullptr;

bool updateInProgress = false;
size_t updateSize = 0;
size_t writtenSize = 0;

class OTACallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        String value = pCharacteristic->getValue();
        
        if (!updateInProgress) {
            // First packet should be JSON with update size
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, value.c_str());
            
            if (!error) {
                updateSize = doc["size"];
                if (DEBUG) {
                    Serial.printf("Starting update of size: %d\n", updateSize);
                }
                
                if (!Update.begin(updateSize)) {
                    if (DEBUG) {
                        Serial.println("Not enough space to begin OTA");
                    }
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
            
            if (DEBUG) {
                Serial.printf("Written: %d/%d\n", writtenSize, updateSize);
            }
            
            if (writtenSize >= updateSize) {
                if (Update.end(true)) {
                    if (DEBUG) {
                        Serial.println("Update Success! Restarting...");
                    }
                    delay(1000);
                    ESP.restart();
                } else {
                    if (DEBUG) {
                        Serial.println("Update Failed!");
                    }
                }
                updateInProgress = false;
            }
        }
    }
};

void initializeOTAService(BLEServer* server) {
    // Create OTA Service
    otaService = server->createService(OTA_SERVICE_UUID);
    
    // Create BLE Characteristic for OTA
    otaCharacteristic = otaService->createCharacteristic(
        OTA_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    otaCharacteristic->setCallbacks(new OTACallbacks());

    // Create Version Characteristic
    versionCharacteristic = otaService->createCharacteristic(
        VERSION_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    versionCharacteristic->setValue(FIRMWARE_VERSION);

    // Start the service
    otaService->start();
    
    if (DEBUG) {
        Serial.println("OTA Service initialized");
        Serial.printf("Current firmware version: %s\n", FIRMWARE_VERSION);
    }
} 