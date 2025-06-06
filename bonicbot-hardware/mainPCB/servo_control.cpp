#include "servo_control.h"

// Global servo controller instance
SMS_STS st;

// Initialize servo system
void initializeServos(HardwareSerial &servoSerial)
{
  servoSerial.begin(1000000, SERIAL_8N1, SERVOS_RXD, SERVOS_TXD);
  st.pSerial = &servoSerial;
  if (DEBUG)
    Serial.println("Servo serial initialized");
}

// Helper function to get the angle limits for a specific servo
void getAngleLimits(int servoIndex, float &minAngle, float &maxAngle)
{
  switch (servoIndex)
  {
  case RIGHT_GRIPPER:
    minAngle = -90.0;
    maxAngle = 90.0;
    break;
  case RIGHT_WRIST:
    minAngle = -90.0;
    maxAngle = 90.0;
    break;
  case RIGHT_ELBOW:
    minAngle = 0;
    maxAngle = 90.0;
    break;
  case RIGHT_SHOLDER_PITCH:
    minAngle = -180;
    maxAngle = 45;
    break;
  case RIGHT_SHOLDER_YAW:
    minAngle = -90;
    maxAngle = 90;
    break;
  case RIGHT_SHOLDER_ROLL:
    minAngle = -144;
    maxAngle = 3;
    break;
  case LEFT_GRIPPER:
    minAngle = -90.0;
    maxAngle = 90.0;
    break;
  case LEFT_WRIST:
    minAngle = -90.0;
    maxAngle = 90.0;
    break;
  case LEFT_ELBOW:
    minAngle = -90.0;
    maxAngle = 0.0;
    break;
  case LEFT_SHOLDER_PITCH:
    minAngle = -45;
    maxAngle = 180;
    break;
  case LEFT_SHOLDER_YAW:
    minAngle = -90;
    maxAngle = 90;
    break;
  case LEFT_SHOLDER_ROLL:
    minAngle = -3.0;
    maxAngle = 144.0;
    break;
  case HEAD_PAN:
    minAngle = -90.0;
    maxAngle = 90.0;
    break;
  case HEAD_TILT:
    minAngle = -38.0;
    maxAngle = 45.0;
    break;
  default:
    minAngle = -90.0;
    maxAngle = 90.0;
    break;
  }
}

// Get which body part group a servo belongs to
String getServoGroup(int servoIndex)
{
  if (servoIndex >= RIGHT_GRIPPER && servoIndex <= RIGHT_SHOLDER_PITCH)
  {
    return RIGHT_HAND_GROUP;
  }
  else if (servoIndex >= LEFT_GRIPPER && servoIndex <= LEFT_SHOLDER_PITCH)
  {
    return LEFT_HAND_GROUP;
  }
  else
  {
    return HEAD_GROUP;
  }
}

// Find servo index by name
int findServoByName(const String &name)
{
  for (int i = 0; i < TOTAL_SERVOS; i++)
  {
    if (name == SERVO_NAMES[i])
    {
      return i;
    }
  }
  return -1; // Not found
}

// Convert angle to servo position using standard mapping:
// 0 degrees = 2048, -90 degrees = 0, +90 degrees = 4096
s16 angleToServoPos(float angle, int servoIndex)
{
  // Clamp the input angle to the permitted range for this servo
  float minAngle, maxAngle;
  getAngleLimits(servoIndex, minAngle, maxAngle);
  angle = constrain(angle, minAngle, maxAngle);

  // Standard mapping: 0 degrees = 2048, -180 degrees = 0, +180 degrees = 4096
  return (s16)(2048 + (angle * (2048 / 180.0)));
}

// Convert servo position to angle using the standard mapping
float servoPosToAngle(s16 pos, int servoIndex)
{

  // Standard mapping: 2048 = 0 degrees, 0 = -180 degrees, 4096 = +180 degrees
  float angle = (pos - 2048) * (180.0 / 2048);
  float minAngle,
      maxAngle;
  getAngleLimits(servoIndex, minAngle, maxAngle);
  return constrain(angle, minAngle, maxAngle);
}

void invertAngles(float *angles, int count)
{
  for (int i = 0; i < count; i++)
  {
    angles[i] = -angles[i];
  }
}

// Update a single servo
bool updateSingleServo(int servoIndex, float angle)
{
  // Store angle for group updates
  String group = getServoGroup(servoIndex);
  if (group == RIGHT_HAND_GROUP)
  {
    angle = -angle;
  }

  // Convert angle to position
  s16 targetPos = angleToServoPos(angle, servoIndex);

  // Send command to the servo
  int result = st.WritePosEx(SERVO_IDS[servoIndex], targetPos,
                             SERVO_SPEED[servoIndex], SERVO_ACC[servoIndex]);

  if (result != 1)
  {
    Serial.print("ERROR: Failed to update servo #");
    Serial.println(SERVO_IDS[servoIndex]);
    return false;
  }
  else if (DEBUG)
  {
    Serial.print("Updated servo #");
    Serial.print(SERVO_IDS[servoIndex]);
    Serial.print(" (");
    Serial.print(SERVO_NAMES[servoIndex]);
    Serial.print(") to angle ");
    Serial.print(angle);
    Serial.print(" (position ");
    Serial.print(targetPos);
    Serial.println(")");
  }

  return true;
}

void setServoMiddle(int servoIndex)
{
  if (servoIndex > 0)
  {
    st.CalibrationOfs(SERVO_IDS[servoIndex]);
  }

  if (DEBUG)
  {
    Serial.print("Set servo #");
    Serial.print(SERVO_IDS[servoIndex]);
    Serial.print(" (");
    Serial.print(SERVO_NAMES[servoIndex]);
    Serial.println(") to middle position (calibration offset)");
  }
}

void releaseServo(int servoIndex)
{
  if (servoIndex > 0)
  {
    st.EnableTorque(SERVO_IDS[servoIndex], false);
  }

  if (DEBUG)
  {
    Serial.print("Released servo #");
    Serial.println(SERVO_IDS[servoIndex]);
  }
}

// Update a group of servos synchronously
bool updateServoGroup(byte *indices, int count, float *angles)
{
  // Prepare arrays for the SyncWritePosEx function
  byte servos[count];
  s16 positions[count];
  u16 speeds[count];
  byte accs[count];

  for (int i = 0; i < count; i++)
  {
    int index = indices[i];
    servos[i] = SERVO_IDS[index];
    positions[i] = angleToServoPos(angles[i], index);
    speeds[i] = SERVO_SPEED[index];
    accs[i] = SERVO_ACC[index];
  }

  // Update servos synchronously
  st.SyncWritePosEx(servos, count, positions, speeds, accs);

  // For debugging
  if (DEBUG)
  {
    Serial.print("Updated servo group of ");
    Serial.print(count);
    Serial.println(" servos");
  }

  return true;
}

// Update right hand servos as a group
bool updateRightHandServos(float *angles)
{
  invertAngles(angles, 6);
  return updateServoGroup(RIGHT_HAND_INDICES, 6, angles);
}

// Update left hand servos as a group
bool updateLeftHandServos(float *angles)
{
  return updateServoGroup(LEFT_HAND_INDICES, 6, angles);
}

// Update head servos as a group
bool updateHeadServos(float *angles)
{
  return updateServoGroup(HEAD_INDICES, 2, angles);
}

bool readServoBasedOnGroup(JsonObject &servoGroup, String groupName)
{
  bool success = true;
  int startIndex = -1;
  int endIndex = -1;

  // Determine start and end indices based on group name
  if (groupName == RIGHT_HAND_GROUP)
  {
    startIndex = RIGHT_GRIPPER;
    endIndex = RIGHT_SHOLDER_PITCH;
  }
  else if (groupName == LEFT_HAND_GROUP)
  {
    startIndex = LEFT_GRIPPER;
    endIndex = LEFT_SHOLDER_PITCH;
  }
  else if (groupName == HEAD_GROUP)
  {
    startIndex = HEAD_PAN;
    endIndex = HEAD_TILT;
  }
  else
  {
    Serial.print("ERROR: Unknown group name - ");
    Serial.println(groupName);
    return false;
  }

  // Read all servos in the specified group
  for (int i = startIndex; i <= endIndex; i++)
  {
    // Create servo object with the exact name from Dart
    JsonObject servo = servoGroup.createNestedObject(SERVO_NAMES[i]);

    // Read servo feedback
    if (st.FeedBack(SERVO_IDS[i]) != -1)
    {
      int pos = st.ReadPos(SERVO_IDS[i]);
      int speed = st.ReadSpeed(SERVO_IDS[i]);
      int load = st.ReadLoad(SERVO_IDS[i]);
      int temp = st.ReadTemper(SERVO_IDS[i]);

      // Convert position to angle
      float angle = servoPosToAngle(pos, i);

      // Check if we need to negate the angle (as in the right hand example)
      // You may need to adjust this logic based on your system's conventions
      if (groupName == RIGHT_HAND_GROUP)
      {
        angle = -angle; // Negate for right hand group as in the example
      }

      // Add values to match the ServoModel.fromRobotJson format
      servo["angle"] = angle;
      servo["speed"] = speed;
      servo["load"] = load;
      servo["temp"] = temp;
      servo["id"] = SERVO_IDS[i];
    }
    else
    {
      servo["error"] = true;
      success = false;
    }
  }

  return success;
}

// the servo object must contain the servo name as type
bool readSingleServoData(JsonObject &servo)
{
  if (servo.containsKey("type"))
  {
    bool success = true;

    int servoIndex = findServoByName(servo["type"]);

    if (servoIndex > 0)
    {

      if (st.FeedBack(SERVO_IDS[servoIndex]) != -1)
      {
        int pos = st.ReadPos(SERVO_IDS[servoIndex]);
        int speed = st.ReadSpeed(SERVO_IDS[servoIndex]);
        int load = st.ReadLoad(SERVO_IDS[servoIndex]);
        int temp = st.ReadTemper(SERVO_IDS[servoIndex]);

        // Convert position to angle
        float angle = servoPosToAngle(pos, servoIndex);

        // Add values to match the ServoModel.fromRobotJson format
        servo["angle"] = angle;
        servo["speed"] = speed;
        servo["load"] = load;
        servo["temp"] = temp;
        // servo["id"] = SERVO_IDS[servoIndex];
      }
      else
      {
        servo["error"] = "Cannot Read";
        success = false;
      }
    }
    // Read servo feedback
    else
    {
      servo["error"] = "Invalid ID";
      success = false;
    }

    return success;
  }
  servo["error"] = "Invalid ID";
  return false;
}

// // New function to read right hand servo data
bool readRightHandServoData(JsonObject &rightHand)
{

  return readServoBasedOnGroup(rightHand, RIGHT_HAND_GROUP);
}

// New function to read left hand servo data
bool readLeftHandServoData(JsonObject &leftHand)
{
  return readServoBasedOnGroup(leftHand, LEFT_HAND_GROUP);
}

// New function to read head servo data
bool readHeadServoData(JsonObject &head)
{
  return readServoBasedOnGroup(head, HEAD_GROUP);
}

void processServoCommandsOfGroup(JsonObject servos, String group)
{
  // Mark beginning of servo command
  servoCommandInProgress = true;

  // Arrays to store servo angles for group updates (maximum size needed is 6 for hands)
  float servoAngles[6] = {0};
  int groupSize = 0;
  int groupStartIndex = 0;

  // Determine group parameters
  if (group == RIGHT_HAND_GROUP)
  {
    groupSize = 6;
    groupStartIndex = RIGHT_GRIPPER;
  }
  else if (group == LEFT_HAND_GROUP)
  {
    groupSize = 6;
    groupStartIndex = LEFT_GRIPPER;
  }
  else if (group == HEAD_GROUP)
  {
    groupSize = 2;
    groupStartIndex = HEAD_PAN;
  }
  else
  {
    // Invalid group
    Serial.print("ERROR: Invalid group specified - ");
    Serial.println(group);
    servoCommandInProgress = false;
    return;
  }

  bool updateAsGroup = false;

  // Process all servo commands using the standardized names
  for (JsonPair kv : servos)
  {
    String servoName = kv.key().c_str();
    int servoIndex = findServoByName(servoName);

    if (servoIndex >= 0)
    {
      // Check if this servo belongs to the specified group
      String servoGroup = getServoGroup(servoIndex);

      if (servoGroup == group)
      {
        JsonObject servoObj = kv.value().as<JsonObject>();

        // Process angle command
        if (servoObj.containsKey("angle"))
        {
          float angle = servoObj["angle"];

          if (DEBUG)
          {
            Serial.print("Setting servo ");
            Serial.print(servoName);
            Serial.print(" to angle ");
            Serial.println(angle);
          }

          // Store angle for group updates
          int relativeIndex = servoIndex - groupStartIndex;
          servoAngles[relativeIndex] = angle;
          updateAsGroup = true;
        }

        // Process speed command
        if (servoObj.containsKey("speed"))
        {
          SERVO_SPEED[servoIndex] = constrain(servoObj["speed"].as<int>(), 0, 1000);
        }

        // Process acceleration command
        if (servoObj.containsKey("acc"))
        {
          SERVO_ACC[servoIndex] = constrain(servoObj["acc"].as<int>(), 0, 255);
        }
      }
    }
  }

  // Now perform updates for the group
  if (updateAsGroup)
  {
    // Update the appropriate group
    bool updateSuccess = false;
    if (group == RIGHT_HAND_GROUP)
    {
      updateSuccess = updateRightHandServos(servoAngles);
    }
    else if (group == LEFT_HAND_GROUP)
    {
      updateSuccess = updateLeftHandServos(servoAngles);
    }
    else if (group == HEAD_GROUP)
    {
      updateSuccess = updateHeadServos(servoAngles);
    }

    if (!updateSuccess)
    {
      Serial.print("ERROR: Failed to update ");
      Serial.print(group);
      Serial.println(" servos");
    }
  }

  // Mark end of servo command
  servoCommandInProgress = false;
}

// Process command for a single servo
void processServoCommandOfSingle(JsonObject servoObj)
{
  if (servoObj.containsKey("id"))
  {
    String servoName = servoObj["id"];
    int servoIndex = findServoByName(servoName);

    if (servoIndex >= 0)
    {
      // Process angle command
      if (servoObj.containsKey("angle"))
      {
        float angle = servoObj["angle"];

        if (DEBUG)
        {
          Serial.print("Setting single servo ");
          Serial.print(servoName);
          Serial.print(" to angle ");
          Serial.println(angle);
        }

        // Process speed command (if present)
        if (servoObj.containsKey("speed"))
        {
          SERVO_SPEED[servoIndex] = constrain(servoObj["speed"].as<int>(), 0, 1000);
        }

        // Process acceleration command (if present)
        if (servoObj.containsKey("acc"))
        {
          SERVO_ACC[servoIndex] = constrain(servoObj["acc"].as<int>(), 0, 255);
        }

        // Update the servo
        if (!updateSingleServo(servoIndex, angle))
        {
          Serial.print("ERROR: Failed to update servo ");
          Serial.println(servoName);
        }
      }
      else if (servoObj.containsKey("setMiddle"))
      {
        setServoMiddle(servoIndex);
      }
      else if (servoObj.containsKey("release"))
      {
        releaseServo(servoIndex);
      }
      else
      {
        Serial.print("ERROR: No angle specified for servo ");
        Serial.println(servoName);
      }
    }
  }
  else
  {
    Serial.println("ERROR: Invalid servo name - ");
  }
}