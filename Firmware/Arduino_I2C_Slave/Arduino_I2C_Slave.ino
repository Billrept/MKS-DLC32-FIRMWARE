/*
 * Arduino I2C Slave for MKS DLC32 V2.1
 * 
 * This sketch implements an I2C slave interface to work with GRBL_ESP32 on MKS DLC32 V2.1
 * The Arduino responds to I2C requests from the ESP32 master with JSON data containing
 * the current machine mode.
 * 
 * Hardware configuration:
 * - Arduino UNO or MEGA as I2C slave at address 0x08
 * - SDA connected to ESP32 IO0
 * - SCL connected to ESP32 IO4
 * 
 * Mode Selection:
 * - Pin 4 HIGH = Spindle mode
 * - Pin 5 HIGH = Laser mode
 * - Pin 6 HIGH = Drawing mode
 * - All pins LOW = None mode
 */

#include <Wire.h>
#include <ArduinoJson.h>

// I2C slave address (must match the master's request address)
#define I2C_SLAVE_ADDRESS 0x08

// Available machine modes
#define MODE_NONE     0  // No specific mode
#define MODE_SPINDLE  1  // Spindle mode (CNC)
#define MODE_LASER    2  // Laser mode
#define MODE_DRAWING  3  // Drawing/pen plotter mode

// Current mode - default to MODE_NONE
uint8_t currentMode = MODE_NONE;

// JSON buffer for response
char jsonBuffer[32];

// Flag to indicate if mode has changed and needs to be reported
bool modeChanged = false;

// Mode selection pins
const int PIN_SPINDLE = 4;  // Pin 4 HIGH = Spindle mode
const int PIN_LASER = 5;    // Pin 5 HIGH = Laser mode
const int PIN_DRAWING = 6;  // Pin 6 HIGH = Drawing mode

// Status LED pin
const int LED_STATUS = 13;  // Arduino onboard LED for status

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("Arduino I2C Slave Initializing...");
  
  // Initialize I2C as slave
  Wire.begin(I2C_SLAVE_ADDRESS);
  
  // Register onRequest event handler
  Wire.onRequest(requestEvent);
  
  // Setup mode selection pins with pull-down resistors
  pinMode(PIN_SPINDLE, INPUT);
  pinMode(PIN_LASER, INPUT);
  pinMode(PIN_DRAWING, INPUT);
  
  // Setup status LED
  pinMode(LED_STATUS, OUTPUT);
  
  // Initialize LED state (blink once to indicate startup)
  digitalWrite(LED_STATUS, HIGH);
  delay(500);
  digitalWrite(LED_STATUS, LOW);
  
  // Prepare initial JSON response
  updateJsonBuffer();
  
  Serial.println("Arduino I2C Slave Ready at address 0x08");
  Serial.println("Pin 4 HIGH = Spindle mode");
  Serial.println("Pin 5 HIGH = Laser mode");
  Serial.println("Pin 6 HIGH = Drawing mode");
}

void loop() {
  // Check pins for mode changes
  checkModePins();
  
  // If mode has changed, update JSON
  if (modeChanged) {
    updateJsonBuffer();
    modeChanged = false;
    
    // Blink status LED to indicate mode change
    digitalWrite(LED_STATUS, HIGH);
    delay(200);
    digitalWrite(LED_STATUS, LOW);
    
    Serial.print("Mode changed to: ");
    Serial.println(getModeString());
  }
  
  // Small delay to prevent excessive CPU usage
  delay(50);
}

// Function called when ESP32 master requests data
void requestEvent() {
  // Send the JSON string
  Wire.write(jsonBuffer);
  
  // Quick flash of LED to show I2C activity
  digitalWrite(LED_STATUS, HIGH);
  // LED will be turned off in the main loop
}

// Check input pins and update mode if needed
void checkModePins() {
  uint8_t newMode = MODE_NONE;
  
  // Check pins in priority order (if multiple pins are HIGH)
  if (digitalRead(PIN_SPINDLE) == HIGH) {
    newMode = MODE_SPINDLE;
  } else if (digitalRead(PIN_LASER) == HIGH) {
    newMode = MODE_LASER;
  } else if (digitalRead(PIN_DRAWING) == HIGH) {
    newMode = MODE_DRAWING;
  }
  
  // If mode has changed, set flag
  if (newMode != currentMode) {
    currentMode = newMode;
    modeChanged = true;
  }
  
  // Turn off status LED (in case it was turned on by requestEvent)
  digitalWrite(LED_STATUS, LOW);
}

// Get string representation of current mode
const char* getModeString() {
  switch (currentMode) {
    case MODE_SPINDLE:
      return "spindle";
    case MODE_LASER:
      return "laser";
    case MODE_DRAWING:
      return "drawing";
    default:
      return "none";
  }
}

// Update the JSON buffer with current mode
void updateJsonBuffer() {
  // Create JSON document
  StaticJsonDocument<32> doc;
  
  // Set the mode value
  doc["mode"] = getModeString();
  
  // Serialize to the buffer
  serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
}