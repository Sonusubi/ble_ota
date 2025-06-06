#include <Arduino.h>
#include <HardwareSerial.h>
#define TFL_RX 13 // TF-Luna TX -> ESP32 RX
#define TFL_TX 12 // TF-Luna RX -> ESP32 TX
void setup()
{
  Serial.begin(115200);                              // Debug Serial Monitor
  Serial2.begin(115200, SERIAL_8N1, TFL_RX, TFL_TX); // TF-Luna Serial
  Serial.println("TF-Luna Sensor Initialized");
}
void loop()
{
  static uint8_t buffer[9];
  if (Serial2.available())
  {
    if (Serial2.read() == 0x59)
    { // First frame header
      if (Serial2.read() == 0x59)
      { // Second frame header
        for (int i = 0; i < 7; i++)
        {
          buffer[i] = Serial2.read();
        }
        uint16_t distance = buffer[0] + buffer[1] * 256;
        uint16_t strength = buffer[2] + buffer[3] * 256;
        uint8_t temp = buffer[4] + buffer[5] * 256;
        temp = temp / 8 - 256;
        if (Serial.available())
        {
          String query = Serial.readStringUntil('\n');
          query.trim();
          if (query.equalsIgnoreCase("distance"))
          {
            // Transmit the latest distance when queried
            if (distance != -1)
            {
              Serial.print("Distance: ");
              Serial.print(distance);
              Serial.println(" cm");
            }
            else
            {
              Serial.println("Failed to read distance.");
            }
          }
          else
          {
            Serial.println("Unknown query. Send 'distance' to get the distance.");
          }
        }
      }
    }
  }
}