#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include <CytronMotorDriver.h>
#include <ddsm_ctrl.h>
#include <ArduinoJson.h>
#include "configs.h"

// Initialize motor system based on configuration
void initializeMotors(HardwareSerial &motorSerial);

// Set motor speeds (values between -255 and 255)
void setMotorSpeeds(int motor1Speed, int motor2Speed);

// Read motor status into JSON object
bool readBaseMotorData(JsonObject &motors);

// Process motor commands from JSON
void processMotorCommands(JsonObject motors);

#endif // MOTOR_CONTROL_H