#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include <Arduino.h>
#include <SCServo.h>
#include <ArduinoJson.h>
#include "configs.h"

// Initialize servo system
void initializeServos(HardwareSerial &servoSerial);

// Update a single servo
bool updateSingleServo(int servoIndex, float angle);

// Update right hand servos as a group
bool updateRightHandServos(float *angles);

// Update left hand servos as a group
bool updateLeftHandServos(float *angles);

// Update head servos as a group
bool updateHeadServos(float *angles);

// Update all servos at once
// bool updateAllServos(float *angles);

// Read servo data into JSON object
// bool readServoData(JsonObject &servos);

bool readSingleServoData(JsonObject &servo);

bool readRightHandServoData(JsonObject &rightHand);

bool readLeftHandServoData(JsonObject &rightHand);

bool readHeadServoData(JsonObject &rightHand);

// Process servo commands from JSON
void processServoCommands(JsonObject servos);

void processServoCommandOfSingle(JsonObject servoObj);

// bool readServoBasedOnGroup(JsonObject &servoGroup, String groupName);

#endif // SERVO_CONTROL_H