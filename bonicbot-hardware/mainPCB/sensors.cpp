#include "sensors.h"
#include <SoftwareSerial.h>

// Global sensor instances
Daly_BMS_UART *bms = nullptr; // Will be initialized in initializeSensors()
SoftwareSerial headSerial(HEAD_RXD, HEAD_TXD);
// String currentHeadMode;
int lastDistanceReading = 0;
unsigned long lastDistanceReadTime = 0;

// Initialize sensor systems
void initializeSensors(HardwareSerial &bmsSerial)
{
  // Initialize BMS
  bmsSerial.begin(9600, SERIAL_8N1, BMS_RXD, BMS_TXD);
  bms = new Daly_BMS_UART(bmsSerial);
  bms->Init();
  if (DEBUG)
    Serial.println("BMS initialized");

  // Initialize head board serial
  headSerial.begin(9600);

  if (DEBUG)
    Serial.println("Eye board initialized");
}

// Set head board mode (Happy, Sad)
bool setHeadMode(String mode)
{
  // Send the mode command as a string
  headSerial.println(mode);

  // Store current mode
  // currentHeadMode = mode;

  if (DEBUG)
  {
    Serial.print("Head mode set to ");
    Serial.println(mode);
  }

  return true;
}

// Get distance reading from eye board (sends command 3)
int getHeadDistance()
{
  // Don't read too frequently - limit to once every 100ms
  unsigned long now = millis();
  if (now - lastDistanceReadTime < 100)
  {
    return lastDistanceReading;
  }

  // Clear any pending data
  while (headSerial.available())
  {
    headSerial.read();
  }

  // Send the distance command
  headSerial.println("d");

  // Wait for response with timeout
  unsigned long startTime = millis();
  while (headSerial.available() < 1)
  {
    if (millis() - startTime > 500)
    {
      if (DEBUG)
        Serial.println("Timeout waiting for distance");
      return -1;
    }
    delay(10);
  }

  // Read the response
  String response = headSerial.readStringUntil('\n');
  int distance = response.toInt();

  // Store the reading and time
  lastDistanceReading = distance;
  lastDistanceReadTime = now;

  if (DEBUG)
  {
    Serial.print("Head distance: ");
    Serial.println(distance);
  }

  return distance;
}

// Read BMS data into JSON object
bool readBmsData(JsonObject &battery)
{
  if (bms == nullptr)
  {
    battery["error"] = true;
    battery["message"] = "BMS not initialized";
    return false;
  }

  bool success = bms->update();

  battery["voltage"] = bms->get.packVoltage;
  battery["current"] = bms->get.packCurrent;
  battery["soc"] = bms->get.packSOC;
  battery["temp"] = bms->get.tempAverage;

  if (bms->get.packSOC < 0 || bms->get.packSOC > 100)
  {
    battery["error"] = true;
    battery["message"] = "Invalid SOC data";
    return false;
  }

  return success;
}

bool readDistanceData(JsonObject &distance)
{
  int value = getHeadDistance();
  if (value >= 0)
  {
    distance["distance"] = value;
  }
  return true;
}

// Read head board data into JSON object
// bool readHeadData(JsonObject &head)
// {
//   head["mode"] = currentHeadMode;

//   int distance = getHeadDistance();
//   if (distance >= 0)
//   {
//     head["distance"] = distance;
//     return true;
//   }
//   else
//   {
//     head["error"] = true;
//     head["message"] = "Failed to read distance";
//     return false;
//   }
// }

// Read all sensor data into JSON object
// bool readSensorData(JsonObject &sensors)
// {
//   bool success = true;

//   // Add BMS data
//   JsonObject battery = sensors.createNestedObject("battery");
//   if (!readBmsData(battery))
//   {
//     success = false;
//   }

//   // Add eye data
//   JsonObject head = sensors.createNestedObject("head");
//   if (!readHeadData(head))
//   {
//     success = false;
//   }

//   return success;
// }