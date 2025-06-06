#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <BLEDevice.h>
#include <Update.h>
#include <ArduinoJson.h>

// OTA Service and Characteristic UUIDs
#define OTA_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define OTA_CHAR_UUID          "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define VERSION_CHAR_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a9"

extern BLEService* otaService;
extern BLECharacteristic* otaCharacteristic;
extern BLECharacteristic* versionCharacteristic;

// OTA update state
extern bool updateInProgress;
extern size_t updateSize;
extern size_t writtenSize;

void initializeOTAService(BLEServer* server);

#endif // OTA_SERVICE_H 