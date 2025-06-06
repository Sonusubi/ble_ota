#include "communication.h"

// Send response over selected communication channels
void sendResponse(const String &response, const char *characteristicUUID)
{

#if COMM_METHOD == COMM_METHOD_SERIAL || COMM_METHOD == COMM_METHOD_BOTH
    // Send over Serial
    Serial.println(response);
#endif

#if COMM_METHOD == COMM_METHOD_BLE || COMM_METHOD == COMM_METHOD_BOTH
    // Send over BLE if connected
    if (DEBUG)
    {
        Serial.print("BLE send debug: deviceConnected=");
        Serial.println(deviceConnected ? "true" : "false");
        Serial.print("Characteristic UUID: ");
        Serial.println(characteristicUUID ? characteristicUUID : "nullptr");
        Serial.print("Response: ");
        Serial.println(response);
    }

    if (deviceConnected)
    {
        if (characteristicUUID == nullptr || strcmp(characteristicUUID, BATTERY_CHAR_UUID) == 0)
        {
            batteryChar->setValue(response.c_str());
            batteryChar->notify();
        }
        else if (strcmp(characteristicUUID, LEFT_HAND_SERVOS_FEEDBACK_CHAR_UUID) == 0)
        {
            leftHandServosChar->setValue(response.c_str());
            leftHandServosChar->notify();
        }
        else if (strcmp(characteristicUUID, RIGHT_HAND_SERVOS_FEEDBACK_CHAR_UUID) == 0)
        {
            rightHandServosChar->setValue(response.c_str());
            rightHandServosChar->notify();
        }
        else if (strcmp(characteristicUUID, HEAD_HAND_SERVOS_FEEDBACK_CHAR_UUID) == 0)
        {
            headServosChar->setValue(response.c_str());
            headServosChar->notify();
        }
        else if (strcmp(characteristicUUID, DISTANCE_FEEDBACK_CHAR_UUID) == 0)
        {
            distanceChar->setValue(response.c_str());
            distanceChar->notify();
        }
        else if (strcmp(characteristicUUID, BASE_FEEDBACK_CHAR_UUID) == 0)
        {
            baseChar->setValue(response.c_str());
            baseChar->notify();
        }
        else if (strcmp(characteristicUUID, SERVO_FEEDBACK_CHAR_UUID) == 0)
        {
            servoChar->setValue(response.c_str());
            servoChar->notify();
        }
    }
#endif
}

// Send a status update
void sendStatus(const char *statusMsg)
{
    DynamicJsonDocument statusDoc(128);
    statusDoc["status"] = statusMsg;
    statusDoc["timestamp"] = millis();

    String statusJson;
    serializeJson(statusDoc, statusJson);

    sendResponse(statusJson);

    if (DEBUG)
    {
        Serial.print("Status sent: ");
        Serial.println(statusMsg);
    }
}

// Stop continuous data sending
void stopContinuousDataSending(const String &dataType)
{
#if COMM_METHOD == COMM_METHOD_BLE || COMM_METHOD == COMM_METHOD_BOTH
    if (dataType == BATTERY)
    {
        batterySendTicker.detach();
        isContinuousBatteryActive = false;
        if (DEBUG)
            Serial.println("Stopped continuous battery data sending");
    }
    else if (dataType == LEFT_HAND_GROUP)
    {
        leftHandServosSendTicker.detach();
        isContinuousLeftHandServosActive = false;
        if (DEBUG)
            Serial.println("Stopped continuous left hand servo data sending");
    }
    else if (dataType == RIGHT_HAND_GROUP)
    {
        rightHandServosSendTicker.detach();
        isContinuousRightHandServosActive = false;
        if (DEBUG)
            Serial.println("Stopped continuous right hand servo data sending");
    }
    else if (dataType == HEAD_GROUP)
    {
        headServosSendTicker.detach();
        isContinuousHeadServosActive = false;
        if (DEBUG)
            Serial.println("Stopped continuous head servo data sending");
    }
    else if (dataType == BASE)
    {
        baseSendTicker.detach();
        isContinuousBaseActive = false;
        if (DEBUG)
            Serial.println("Stopped continuous base/motor data sending");
    }
    else if (dataType == DISTANCE)
    {
        distanceSendTicker.detach();
        isContinuousDistanceActive = false;
        if (DEBUG)
            Serial.println("Stopped continuous distance data sending");
    }
#endif
}

// Stop all continuous data sending
void stopAllContinuousDataSending()
{
#if COMM_METHOD == COMM_METHOD_BLE || COMM_METHOD == COMM_METHOD_BOTH
    batterySendTicker.detach();
    leftHandServosSendTicker.detach();
    rightHandServosSendTicker.detach();
    headServosSendTicker.detach();
    baseSendTicker.detach();
    distanceSendTicker.detach();
    servoSendTicker.detach();

    isContinuousBatteryActive = false;
    isContinuousLeftHandServosActive = false;
    isContinuousRightHandServosActive = false;
    isContinuousHeadServosActive = false;
    isContinuousBaseActive = false;
    isContinuousDistanceActive = false;
    isContinousServoActive = false;

    if (DEBUG)
        Serial.println("Stopped all continuous data sending");
#endif
}

void sendDataBasedOnDataType(const String &dataType, int intervalMs, const String servoName)
{
#if COMM_METHOD == COMM_METHOD_BLE || COMM_METHOD == COMM_METHOD_BOTH
    if (!deviceConnected)
    {
        if (DEBUG)
            Serial.println("Cannot start data sending: device not connected");
        return;
    }

    bool isSendOnce = (intervalMs == 0);

    // Ensure the interval is reasonable (not too fast) if we're doing continuous sending
    if (!isSendOnce && intervalMs < 100)
        intervalMs = 100;

    // Function pointers and settings for each data type
    typedef bool (*DataReaderFunc)(JsonObject &);
    DataReaderFunc dataReader = nullptr;
    const char *charUUID = nullptr;
    bool *continuousActiveFlag = nullptr;
    Ticker *dataSendTicker = nullptr;
    int jsonSize = 0;
    String typeName = "";

    // Configure based on data type
    if (dataType == BATTERY)
    {
        dataReader = readBmsData;
        charUUID = BATTERY_CHAR_UUID;
        continuousActiveFlag = &isContinuousBatteryActive;
        dataSendTicker = &batterySendTicker;
        jsonSize = 256;
        typeName = "battery";
    }
    else if (dataType == LEFT_HAND_GROUP)
    {
        dataReader = readLeftHandServoData;
        charUUID = LEFT_HAND_SERVOS_FEEDBACK_CHAR_UUID;
        continuousActiveFlag = &isContinuousLeftHandServosActive;
        dataSendTicker = &leftHandServosSendTicker;
        jsonSize = 1024;
        typeName = "left hand servos";
    }
    else if (dataType == RIGHT_HAND_GROUP)
    {
        dataReader = readRightHandServoData;
        charUUID = RIGHT_HAND_SERVOS_FEEDBACK_CHAR_UUID;
        continuousActiveFlag = &isContinuousRightHandServosActive;
        dataSendTicker = &rightHandServosSendTicker;
        jsonSize = 1024;
        typeName = "right hand servos";
    }
    else if (dataType == HEAD_GROUP)
    {
        dataReader = readHeadServoData;
        charUUID = HEAD_HAND_SERVOS_FEEDBACK_CHAR_UUID;
        continuousActiveFlag = &isContinuousHeadServosActive;
        dataSendTicker = &headServosSendTicker;
        jsonSize = 512;
        typeName = "head servos";
    }
    else if (dataType == BASE)
    {
        dataReader = readBaseMotorData;
        charUUID = BASE_FEEDBACK_CHAR_UUID;
        continuousActiveFlag = &isContinuousBaseActive;
        dataSendTicker = &baseSendTicker;
        jsonSize = 256;
        typeName = "base";
    }
    else if (dataType == DISTANCE)
    {
        dataReader = readDistanceData;
        charUUID = DISTANCE_FEEDBACK_CHAR_UUID;
        continuousActiveFlag = &isContinuousDistanceActive;
        dataSendTicker = &distanceSendTicker;
        jsonSize = 512;
        typeName = "distance";
    }
    else if (dataType == SERVO)
    {
        dataReader = readSingleServoData;
        charUUID = SERVO_FEEDBACK_CHAR_UUID;
        continuousActiveFlag = &isContinousServoActive;
        dataSendTicker = &servoSendTicker;
        jsonSize = 512;
        typeName = servoName;
    }
    else
    {
        if (DEBUG)
            Serial.println("Unknown data type requested");
        return;
    }

    // Create local copies for the lambda capture
    DataReaderFunc readerFunc = dataReader;
    const char *uuid = charUUID;
    int docSize = jsonSize;
    String name = typeName;
    // String sName = servoName;

    // Handle one-time sending
    if (isSendOnce)
    {
        DynamicJsonDocument doc(docSize);
        JsonObject root = doc.to<JsonObject>();
        // if (name == "servo")
        root["type"] = name;
        readerFunc(root);
        root["timestamp"] = millis();
        root["continuous"] = false;

        String json;
        serializeJson(doc, json);
        sendResponse(json, uuid);

        if (DEBUG)
            Serial.println("One-time " + name + " data sent");
    }
    // Handle continuous sending
    else
    {
        // Stop any existing timer first
        if (*continuousActiveFlag)
        {
            dataSendTicker->detach();
        }

        // Set up new ticker for data
        dataSendTicker->attach_ms(intervalMs, [readerFunc, uuid, docSize, name]()
                                  {
                DynamicJsonDocument doc(docSize);
                JsonObject root = doc.to<JsonObject>();
                // if (name == "servo")
                root["id"] = name;
                readerFunc(root);
                root["timestamp"] = millis();
                root["continuous"] = true;

                String json;
                serializeJson(doc, json);
                sendResponse(json, uuid);

                if (DEBUG)
                    Serial.println("Continuous " + name + " data sent"); });

        *continuousActiveFlag = true;

        if (DEBUG)
        {
            Serial.print("Started continuous " + name + " data sending with interval: ");
            Serial.print(intervalMs);
            Serial.println("ms");
        }
    }

#endif
}