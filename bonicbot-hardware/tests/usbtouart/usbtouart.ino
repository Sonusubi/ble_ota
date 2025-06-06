#include <HardwareSerial.h>

HardwareSerial mySerial(2);  // Use UART2 (GPIO16 & GPIO17)

void setup() {
    Serial.begin(9600);   // USB Serial
    mySerial.begin(9600, SERIAL_8N1, 37, 38);  // External UART
}

void loop() {
    // Forward data from USB to UART
    if (Serial.available()) {
        mySerial.write(Serial.read());
    }

    // Forward data from UART to USB
    if (mySerial.available()) {
        Serial.write(mySerial.read());
    }
}
