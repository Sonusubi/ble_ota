#include "motor_control.h"

// Global motor control instances
#if MOTOR_TYPE == MOTOR_TYPE_CYTRON
CytronMD leftMotor(PWM_DIR, MOTOR1_PWM_PIN, MOTOR1_DIR_PIN);
CytronMD rightMotor(PWM_DIR, MOTOR2_PWM_PIN, MOTOR2_DIR_PIN);
#elif MOTOR_TYPE == MOTOR_TYPE_DDSM
DDSM_CTRL dc;
#endif

// Initialize motor system based on configuration
void initializeMotors(HardwareSerial &motorSerial)
{
#if MOTOR_TYPE == MOTOR_TYPE_CYTRON
  // Cytron motors just need to be set to zero initially
  leftMotor.setSpeed(0);
  rightMotor.setSpeed(0);
  if (DEBUG)
    Serial.println("Cytron motors initialized");
#elif MOTOR_TYPE == MOTOR_TYPE_DDSM
  // DDSM motors require serial communication
  motorSerial.begin(DDSM_BAUDRATE, SERIAL_8N1, MOTOR_RX, MOTOR_TX);
  dc.pSerial = &motorSerial;
  dc.set_ddsm_type(115);
  dc.clear_ddsm_buffer();
  if (DEBUG)
    Serial.println("DDSM motors initialized");
#endif
}

// Set motor speeds (values between -255 and 255)
void setMotorSpeeds(int leftSpeed, int rightSpeed)
{
  // Apply safety limits
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  if (DEBUG)
  {
    Serial.print("Setting motor speeds: Left=");
    Serial.print(leftSpeed);
    Serial.print(", Right=");
    Serial.println(rightSpeed);
  }

#if MOTOR_TYPE == MOTOR_TYPE_CYTRON
  // Use Cytron motor controller
  leftMotor.setSpeed(leftSpeed);
  rightMotor.setSpeed(rightSpeed);
#elif MOTOR_TYPE == MOTOR_TYPE_DDSM
  // Use DDSM motor controller (send command multiple times for reliability)
    dc.ddsm_ctrl(1, leftSpeed, 1); // Motor 1
    dc.ddsm_ctrl(2, -rightSpeed, 1); // Motor 2
#endif
}

// Read motor status into JSON object - Formatted to match BaseModel in Dart
bool readBaseMotorData(JsonObject &base)
{
  // Create left and right motor objects to match Dart's BaseModel
  JsonObject leftMotor = base.createNestedObject("leftMotor");
  JsonObject rightMotor = base.createNestedObject("rightMotor");

  leftMotor["id"] = "left";
  leftMotor["minSpeed"] = -255;
  leftMotor["maxSpeed"] = 255;

  rightMotor["id"] = "right";
  rightMotor["minSpeed"] = -255;
  rightMotor["maxSpeed"] = 255;

  // Add type information based on configured motor type
#if MOTOR_TYPE == MOTOR_TYPE_CYTRON
  leftMotor["type"] = "GearMotor";
  rightMotor["type"] = "GearMotor";

  leftMotor["speed"] = 0;
  rightMotor["speed"] = 0;
#elif MOTOR_TYPE == MOTOR_TYPE_DDSM
  leftMotor["type"] = "DDSM115";
  rightMotor["type"] = "DDSM115";

  leftMotor["speed"] = 0; // Impliment this for DDSM115
  rightMotor["speed"] = 0;
#endif

  return true;
}

// Process motor commands from JSON - Expecting Dart's BaseModel format
void processMotorCommands(JsonObject base)
{
  int leftSpeed = 0;
  int rightSpeed = 0;

  // Check for left motor - using naming from Dart's BaseModel
  if (base.containsKey("leftMotor"))
  {
    JsonObject leftMotorObj = base["leftMotor"];
    if (leftMotorObj.containsKey("speed"))
    {
      leftSpeed = leftMotorObj["speed"];
    }
  }

  // Check for right motor - using naming from Dart's BaseModel
  if (base.containsKey("rightMotor"))
  {
    JsonObject rightMotorObj = base["rightMotor"];
    if (rightMotorObj.containsKey("speed"))
    {
      rightSpeed = rightMotorObj["speed"];
    }
  }

  // Apply motor commands
  setMotorSpeeds(leftSpeed, rightSpeed);
}