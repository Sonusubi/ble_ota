// Include necessary libraries and define global variables
#include <SPI.h>
#include <TFT_eSPI.h>
#include "EYEA.h"
#include <HardwareSerial.h> // Include for TF-Luna sensor

TFT_eSPI tft;           // A single instance is used for 1 or 2 displays
TaskHandle_t tfLunaTaskHandle;
#define device_A_CS  5

// TF-Luna sensor pins
#define TFL_RX 13  // TF-Luna TX -> ESP32 RX
#define TFL_TX 12  // TF-Luna RX -> ESP32 TX
#define UART_RX 10 // UART RX pin
#define UART_TX 11 // UART TX pin
// FOR BONICBOT 2 -
// #define UART_RX 11 // UART RX pin
// #define UART_TX 10 // UART TX pin

// Define some extra colors for display
#define BLACK        0x0000
#define BLUE         0x001F
#define RED          0xF800
#define GREEN        0x07E0
#define CYAN         0x07FF
#define MAGENTA      0xF81F
#define YELLOW       0xFFE0
#define WHITE        0xFFFF
#define ORANGE       0xFBE0
#define GREY         0x84B5
#define BORDEAUX     0xA000
#define DINOGREEN    0x2C86
#define WHITE        0xFFFF

int frameTime = 70; // Frame delay time in milliseconds
int j; // General-purpose variable

// Define buffer size for pixel rendering
#define BUFFER_SIZE 1024 // 128 to 1024 seems optimum

#ifdef USE_DMA
  #define BUFFERS 2      // 2 toggle buffers with DMA
#else
  #define BUFFERS 1      // 1 buffer for no DMA
#endif

// Define pixel rendering buffer and DMA buffer selection
uint16_t pbuffer[BUFFERS][BUFFER_SIZE]; // Pixel rendering buffer
bool     dmaBuf   = 0;                  // DMA buffer selection

// Define a struct for eye configuration
typedef struct {
  int8_t  select;       // Pin numbers for each eye's screen select line
  int8_t  wink;         // Wink button pin (or -1 if none)
  uint8_t rotation;     // Display rotation
  int16_t xposition;    // Position of eye on the screen
} eyeInfo_t;

#include "config.h"     // Configuration file inclusion

// External function declarations
extern void user_setup(void); // Functions in the user*.cpp files
extern void user_loop(void);

// Define screen dimensions
#define SCREEN_X_START 0
#define SCREEN_X_END   SCREEN_WIDTH   // Eye width
#define SCREEN_Y_START 0
#define SCREEN_Y_END   SCREEN_HEIGHT  // Eye height

// Define a state machine for eye blinks/winks
#define NOBLINK 0       // Not currently engaged in a blink
#define ENBLINK 1       // Eyelid is currently closing
#define DEBLINK 2       // Eyelid is currently opening

typedef struct {
  uint8_t  state;       // Blink state (NOBLINK/ENBLINK/DEBLINK)
  uint32_t duration;    // Duration of blink state (microseconds)
  uint32_t startTime;   // Time of last state change (microseconds)
} eyeBlink;

// Define a structure for each eye
struct {
  int16_t   tft_cs;     // Chip select pin for each display
  eyeBlink  blink;      // Current blink/wink state
  int16_t   xposition;  // X position of eye image
} eye[NUM_EYES];

uint32_t startTime;  // Variable for FPS indicator

// TF-Luna sensor variables
static uint8_t buffer[9];
uint16_t distance = 0; // Distance measured by the sensor
uint16_t strength = 0; // Signal strength of the sensor
int8_t temp = 0;       // Temperature reading from the sensor

// Function to demonstrate eye rendering
void Demo_1()
{
  tft.setSwapBytes(false); // Ensure correct byte order
  updateEye(); // Update eye rendering
}

// Function to demonstrate image rendering on the display
void Demo_2()
{
  // Render a sequence of images on the display
  tft.setSwapBytes(false);           // Ensure correct byte order
  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160,160,gImage_A1);
  digitalWrite (device_A_CS, HIGH); 
  
  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A2);
  digitalWrite (device_A_CS, HIGH); 

  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A3);
  digitalWrite (device_A_CS, HIGH); 
   
  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A4);
  digitalWrite (device_A_CS, HIGH); 

  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A5);
  digitalWrite (device_A_CS, HIGH); 

  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A6);
  digitalWrite (device_A_CS, HIGH); 
   
  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A7);
  digitalWrite (device_A_CS, HIGH); 
  
  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A8);
  digitalWrite (device_A_CS, HIGH);  
  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A9);
  digitalWrite (device_A_CS, HIGH); 
   
  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A10);
  digitalWrite (device_A_CS, HIGH); 
   
  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A11);
  digitalWrite (device_A_CS, HIGH);  
  delay (frameTime);

  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160, 160,gImage_A12);
  digitalWrite (device_A_CS, HIGH); 
  delay (frameTime);
}

// Function to demonstrate another rendering mode
void Demo_3()
{
  tft.startWrite();
  tft.setSwapBytes(true);           // Ensure correct byte order
  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160,160,gImage_A13);
  digitalWrite (device_A_CS, HIGH); 
  delay(500);
  tft.endWrite();
  tft.startWrite();
  tft.setSwapBytes(true);           // Ensure correct byte order
  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160,160,gImage_A14);
  digitalWrite (device_A_CS, HIGH); 
  delay(500);
  tft.endWrite();
}

// Function to demonstrate yet another rendering mode
void Demo_4()
{
  tft.startWrite();
  tft.setSwapBytes(true);           // Ensure correct byte order
  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160,160,gImage_A15);
  digitalWrite (device_A_CS, HIGH); 
  delay(500);
  tft.endWrite();
  tft.startWrite();
  tft.setSwapBytes(true);           // Ensure correct byte order
  digitalWrite (device_A_CS, LOW);   
  tft.pushImage (0, 0,160,160,gImage_A16);
  digitalWrite (device_A_CS, HIGH); 
  delay(500);
  tft.endWrite();
}

// Initialization function, runs once at startup
void setup(void) {
  Serial.begin(115200); // Initialize serial communication
  Serial2.begin(115200, SERIAL_8N1, TFL_RX, TFL_TX); // Initialize TF-Luna Serial
  Serial1.begin(9600, SERIAL_8N1, UART_RX, UART_TX); // Initialize UART Serial
  Serial.println("TF-Luna Sensor Initialized");

  tft.writecommand(TFT_MADCTL); // Send initialization command to TFT
  delay(10);
  tft.writedata(TFT_MAD_MX | TFT_MAD_MV );
  Serial.println("Starting");

  // Initialize backlight if defined
  #if defined(DISPLAY_BACKLIGHT) && (DISPLAY_BACKLIGHT >= 0)
    Serial.println("Backlight turned off");
    pinMode(DISPLAY_BACKLIGHT, OUTPUT);
    digitalWrite(DISPLAY_BACKLIGHT, LOW);
  #endif

  user_setup(); // Call user-defined setup function

  initEyes(); // Initialize eye displays

  Serial.println("Initialising displays");
  tft.init(); // Initialize TFT display

  #ifdef USE_DMA
    tft.initDMA(); // Initialize DMA if enabled
  #endif

  // Configure each eye display
  for (uint8_t e = 0; e < NUM_EYES; e++) {
    digitalWrite(eye[e].tft_cs, LOW);
    tft.setRotation(eyeInfo[e].rotation);
    tft.fillScreen(TFT_BLACK);
    digitalWrite(eye[e].tft_cs, HIGH);
  }

  #if defined(DISPLAY_BACKLIGHT) && (DISPLAY_BACKLIGHT >= 0)
    Serial.println("Backlight now on!");
    analogWrite(DISPLAY_BACKLIGHT, BACKLIGHT_MAX);
  #endif

  startTime = millis(); // Record start time for FPS calculation

  Demo_2();

  // Create a task for TF-Luna sensor
  xTaskCreatePinnedToCore(
    tfLunaTask,       // Task function
    "TF-Luna Task",   // Task name
    4096,             // Stack size (in bytes)
    NULL,             // Task input parameter
    1,                // Task priority
    &tfLunaTaskHandle,// Task handle
    0                 // Core to run the task on (core 0)
  );
}

// Main loop, runs continuously after setup()
char i=0;
int k=0;
void loop() {
  if (k == 1) {
    tft.fillScreen(BLACK); // Clear screen
    Demo_1(); // Run Demo 1
  }
  if (k == 2) {
    tft.fillScreen(BLACK); // Clear screen
    Demo_2(); // Run Demo 2
  }
  if (k == 3) {
    tft.fillScreen(BLACK); // Clear screen
    Demo_3(); // Run Demo 3
  }
  if (k == 4) {
    tft.fillScreen(BLACK); // Clear screen
    Demo_4(); // Run Demo 4
  }

  // Check for serial input and handle commands
  if (Serial1.available()) {
    String query = Serial1.readStringUntil('\n');
    Serial.println(query);
    query.trim();
    if (query.equalsIgnoreCase("Normal")) {
      k = 1;
    }
    if (query.equalsIgnoreCase("Angry")) {
      k = 2;
    }
    if (query.equalsIgnoreCase("Happy")) {
      k = 3;
    }
    if (query.equalsIgnoreCase("Sad")) {
      k = 4;
    }
    if (query.equalsIgnoreCase("d")) {
      // Transmit the latest distance when queried
      if (distance != -1) {
        Serial1.print(distance);
        Serial.println(distance);
      } else {
        Serial.println("Failed to read distance.");
      }
    } 
  }
}

// Task function for TF-Luna sensor
void tfLunaTask(void *parameter) {
  while (true) {
    tfLuna(); // Continuously run tfLuna()
    vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay to prevent task starvation
  }
}

// Function to read data from TF-Luna sensor
void tfLuna() {
  static uint8_t buffer[9];
  if (Serial2.available() >= 9) { // Ensure a full data packet is available
    if (Serial2.read() == 0x59) {              // First frame header
      if (Serial2.read() == 0x59) {            // Second frame header
        for (int i = 0; i < 7; i++) {
          buffer[i] = Serial2.read();
        }
        distance = buffer[0] + buffer[1] * 256; // Update global distance variable
        strength = buffer[2] + buffer[3] * 256; // Update global strength variable
        temp = buffer[4] + buffer[5] * 256;     // Update global temp variable
        temp = temp / 8 - 256;
      }
    }
  }
}