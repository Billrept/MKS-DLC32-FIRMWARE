# Arduino I2C Slave for MKS DLC32 V2.1

This project implements an I2C slave interface for an Arduino UNO/MEGA to work with GRBL_ESP32 on the MKS DLC32 V2.1 board. The Arduino responds to I2C requests from the ESP32 master with JSON data containing the current machine mode.

## Hardware Requirements

- Arduino UNO or MEGA
- MKS DLC32 V2.1 board running GRBL_ESP32
- External circuits or switches to set pin states
- Connecting wires

## Wiring Instructions

### I2C Connection
- Connect Arduino SDA pin to ESP32 IO0 (SDA)
- Connect Arduino SCL pin to ESP32 IO4 (SCL)
- Connect Arduino GND to ESP32 GND
- Note: For longer wire runs (>10cm), add 4.7kΩ pull-up resistors on both SDA and SCL lines

### Mode Selection Pins
- Pin 4: When HIGH, sets to SPINDLE mode
- Pin 5: When HIGH, sets to LASER mode
- Pin 6: When HIGH, sets to DRAWING mode
- When all pins are LOW, defaults to NONE mode

Note: If multiple pins are HIGH, priority is given in this order: SPINDLE > LASER > DRAWING

### Status Indicator
- The Arduino's built-in LED (pin 13) is used to indicate:
  - Quick blink at startup
  - Brief flash when I2C communication occurs
  - Short blink when mode changes

## Pin Connection Options

There are several ways to connect the mode pins:

1. **Direct Connection to Switches:**
   - Connect each pin to a switch that connects to 5V when closed
   - Add a 10kΩ pull-down resistor between each pin and GND

2. **External System Control:**
   - Connect pins to controller outputs from another system
   - Ensure voltage levels are appropriate (5V for most Arduinos)

3. **Automatic Mode Detection:**
   - Connect pin 4 to a spindle enable signal
   - Connect pin 5 to a laser enable signal
   - Connect pin 6 to a servo/pen enable signal

## Software Setup

1. Install the ArduinoJson library using the Arduino Library Manager
   - In the Arduino IDE, go to Sketch -> Include Library -> Manage Libraries
   - Search for "ArduinoJson" by Benoit Blanchon
   - Install version 6.x or later

2. Upload the Arduino_I2C_Slave.ino sketch to your Arduino

3. Ensure the ESP32 has the MKS_I2C_Slave module enabled and configured with the same I2C address (0x08)

## Operation

- Set the appropriate pin HIGH to select the machine mode:
  - Pin 4 HIGH = Spindle mode (CNC machining)
  - Pin 5 HIGH = Laser mode (laser engraving)
  - Pin 6 HIGH = Drawing mode (pen plotting)
  - All pins LOW = None mode

- The Arduino will respond to I2C requests from the ESP32 with a JSON string: `{"mode":"laser"}` (or other mode)

- The ESP32 will display the current mode in the UGS console as: `[MODE: laser]`

## Troubleshooting

1. **No communication**
   - Check I2C wiring connections
   - Verify that both devices have common ground
   - Make sure the Arduino is powered on
   - Check that the ESP32 is using pins IO0 and IO4 for I2C

2. **Inconsistent readings**
   - Add pull-up resistors (4.7kΩ) to SDA and SCL lines
   - Reduce wire length or use shielded cable
   - Ensure the ESP32 I2C clock frequency is set to 100kHz

3. **Mode doesn't change**
   - Check pin connections and voltage levels
   - Verify that the Arduino is correctly reading the pin states
   - Check the serial monitor for debugging information

## Serial Debug Output

The Arduino will output debugging information to the Serial monitor at 115200 baud:
- Initialization confirmation
- Mode changes
- I2C activity indicators

## Extending Functionality

You can modify the sketch to add more features:
- Additional machine modes
- Status feedback from the ESP32
- EEPROM storage for remembering the last mode across power cycles
- Advanced mode detection logic