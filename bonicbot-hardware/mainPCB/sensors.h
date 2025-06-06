#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <daly-bms-uart.h>
#include <ArduinoJson.h>
#include "configs.h"

// Initialize sensor systems
void initializeSensors(HardwareSerial &bmsSerial);

// Set head board mode ("Happy", "Sad")
bool setHeadMode(String mode);

// Get distance reading from eye board (sends command 3)
int getHeadDistance();

// Read BMS data into JSON object
bool readBmsData(JsonObject &battery);

bool readDistanceData(JsonObject &distance);

// Read eye board data into JSON object
// bool readHeadData(JsonObject &head);

// Read all sensor data into JSON object
// bool readSensorData(JsonObject &sensors);

#endif // SENSORS_H