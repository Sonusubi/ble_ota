#define TXD1 8 // ESP32 TX to converter RX
#define RXD1 9 // ESP32 RX from converter TX

void setup()
{
    Serial.begin(115200);                          // USB Serial
    Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1); // External Serial1
    delay(1000);

    Serial.println("Serial1 ready. Type something:");
}

void loop()
{
    // Read from Serial (USB) and forward to Serial1
    if (Serial.available())
    {
        String inputFromUSB = Serial.readStringUntil('\n');
        Serial1.println(inputFromUSB);
        Serial.print("Sent to Serial1: ");
        Serial.println(inputFromUSB);
    }

    // Read from Serial1 and print to Serial (USB)
    if (Serial1.available())
    {
        String inputFromSerial1 = Serial1.readStringUntil('\n');
        Serial.print("Received from Serial1: ");
        Serial.println(inputFromSerial1);
    }

    delay(10);
}