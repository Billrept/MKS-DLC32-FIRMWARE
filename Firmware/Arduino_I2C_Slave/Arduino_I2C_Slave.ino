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

// I2C receive buffer
char receiveBuffer[32];
volatile bool newCommandReceived = false;

// Flag to indicate if mode has changed and needs to be reported
bool modeChanged = false;

// Mode selection pins
const int PIN_SPINDLE = 4;  // Pin 4 HIGH = Spindle mode
const int PIN_LASER = 5;    // Pin 5 HIGH = Laser mode
const int PIN_DRAWING = 6;  // Pin 6 HIGH = Drawing mode

// Status LED pin
const int LED_STATUS = 13;  // Arduino onboard LED for status


const int STEP_PIN = 9;
const int DIR_PIN = 8;
const int ENA_PIN = 10;

// Define color positions (steps from origin)
struct ColorPosition {
  const char* name;
  int position;
};

// Color positions array (absolute positions in steps)
ColorPosition colorPositions[] = {
  {"cyan", 0},
  {"magenta", 200},
  {"yellow", 400},
  {"black", 600}
};
const int NUM_COLORS = 4;

// Stepper movement parameters
const int STEP_DELAY = 500;      // Microseconds between steps (500µs = 1000 steps/sec)
const int STEPS_PER_REV = 200;   // Standard NEMA17 has 200 steps per revolution

// Current position
int currentPosition = 0;

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("Arduino I2C Slave Initializing...");
  
  // Initialize I2C as slave
  Wire.begin(I2C_SLAVE_ADDRESS);
  
  // Register I2C event handlers
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);  // Add receive event handler
  
  // Setup mode selection pins
  pinMode(PIN_SPINDLE, INPUT);
  pinMode(PIN_LASER, INPUT);
  pinMode(PIN_DRAWING, INPUT);
  
  // Setup status LED
  pinMode(LED_STATUS, OUTPUT);
  
  // Setup stepper pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  
  // Set initial stepper state
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  digitalWrite(ENA_PIN, HIGH);  // HIGH = disabled initially
  
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
  Serial.println("Ready to receive color commands");
  Serial.println("Using STEP/DIR/ENA control for NEMA17 stepper");
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
  
  // Process new command if received
  if (newCommandReceived) {
    processReceivedCommand();
    newCommandReceived = false;
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

// Function called when ESP32 master sends data
void receiveEvent(int byteCount) {
  int i = 0;
  
  // Clear previous buffer content
  memset(receiveBuffer, 0, sizeof(receiveBuffer));
  
  // Read bytes into buffer
  while (Wire.available() && i < sizeof(receiveBuffer) - 1) {
    receiveBuffer[i++] = Wire.read();
  }
  
  // Ensure null termination
  receiveBuffer[i] = '\0';
  
  // Read any remaining bytes to clear the buffer
  while (Wire.available()) {
    Wire.read();
  }
  
  if (i > 0) {
    newCommandReceived = true;
    Serial.print("Received: ");
    Serial.println(receiveBuffer);
  }
}

// Process the received JSON command
void processReceivedCommand() {
  // Parse JSON
  StaticJsonDocument<32> doc;
  DeserializationError error = deserializeJson(doc, receiveBuffer);
  
  if (error) {
    Serial.print("JSON parsing error: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Check for color command
  if (doc.containsKey("color")) {
    const char* color = doc["color"];
    if (color) {
      Serial.print("Color command received: ");
      Serial.println(color);
      moveToColor(color);
    }
  }
}

// Move stepper to the position for the specified color using STEP/DIR/ENA
void moveToColor(const char* color) {
  // Find target position for the requested color
  int targetPosition = -1;
  
  for (int i = 0; i < NUM_COLORS; i++) {
    if (strcmp(color, colorPositions[i].name) == 0) {
      targetPosition = colorPositions[i].position;
      break;
    }
  }
  
  // If color not found, return
  if (targetPosition == -1) {
    Serial.print("Unknown color: ");
    Serial.println(color);
    return;
  }
  
  // Calculate steps to move
  int stepsToMove = targetPosition - currentPosition;
  
  // Move the stepper motor if needed
  if (stepsToMove != 0) {
    Serial.print("Moving from position ");
    Serial.print(currentPosition);
    Serial.print(" to ");
    Serial.print(targetPosition);
    Serial.print(" (");
    Serial.print(stepsToMove);
    Serial.println(" steps)");
    
    // Enable stepper driver
    digitalWrite(ENA_PIN, LOW);
    
    // Set direction based on whether we need to move forward or backward
    if (stepsToMove > 0) {
      digitalWrite(DIR_PIN, HIGH);  // Forward direction
    } else {
      digitalWrite(DIR_PIN, LOW);   // Backward direction
      stepsToMove = -stepsToMove;   // Make steps positive
    }
    
    // Blink LED while moving
    digitalWrite(LED_STATUS, HIGH);
    
    // Move the stepper by generating step pulses
    for (int i = 0; i < stepsToMove; i++) {
      digitalWrite(STEP_PIN, HIGH);
      delayMicroseconds(STEP_DELAY);
      digitalWrite(STEP_PIN, LOW);
      delayMicroseconds(STEP_DELAY);
    }
    
    // Update current position
    currentPosition = targetPosition;
    
    // Disable stepper when done to prevent heating
    digitalWrite(ENA_PIN, HIGH);
    
    digitalWrite(LED_STATUS, LOW);
    
    Serial.print("Now at color position: ");
    Serial.println(color);
  } else {
    Serial.println("Already at the requested color position");
  }
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