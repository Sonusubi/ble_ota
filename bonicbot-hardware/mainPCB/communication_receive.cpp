#include "communication.h"

// Process incoming command - Command Parser
void processCommand(String command)
{
    if (DEBUG)
    {
        Serial.print("Received command: ");
        Serial.println(command);
    }

    // Create a JSON document for parsing
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, command);
    if (error)
    {
        if (DEBUG)
        {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
        }
        return;
    }

    // Process data request command
    if (doc.containsKey("dataType") && doc.containsKey("commandType") && doc.containsKey("payload") && doc.containsKey("interval"))
    {
        String dataType = doc["dataType"].as<String>();
        String commandType = doc["commandType"].as<String>();
        JsonObject payload = doc["payload"].as<JsonObject>();
        int interval = doc["interval"].as<int>();

        if (commandType == "command")
        {
            if (dataType == LEFT_HAND_GROUP || dataType == RIGHT_HAND_GROUP || dataType == HEAD_GROUP)
            {
                if (dataType == HEAD_GROUP && payload.containsKey("mode"))
                {
                    String modeStr = payload["mode"].as<String>();
                    setHeadMode(modeStr);
                }

                processServoCommandsOfGroup(payload, dataType);
            }
            else if (dataType == BASE)
            {
                processMotorCommands(payload);
            }
            else if (dataType == SERVO)
            {
                // the payload must contain the servoname
                processServoCommandOfSingle(payload);
            }
        }
        else if (commandType == "receiveSingle")
        {
            if (payload.containsKey("id"))
            {
                sendDataBasedOnDataType(dataType, 0, payload["id"]);
            }
            else
            {
                sendDataBasedOnDataType(dataType);
            }
        }
        else if (commandType == "receiveContinuous")
        {
            if (payload.containsKey("id"))
            {
                sendDataBasedOnDataType(dataType, interval, payload["id"]);
            }
            else
            {
                sendDataBasedOnDataType(dataType, interval);
            }
        }
        else if (commandType == "stopReceive")
        {
            if (payload.containsKey("all"))
            {
                stopAllContinuousDataSending();
            }
            stopContinuousDataSending(dataType);
        }
    }
}