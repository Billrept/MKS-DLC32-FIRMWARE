#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "../Grbl.h"

// I2C Configuration
#define I2C_SDA_PIN 0           // I2C SDA pin (IO0)
#define I2C_SCL_PIN 4           // I2C SCL pin (IO4)
#define I2C_CLOCK_FREQ 100000   // 100kHz I2C clock frequency
#define I2C_POLL_INTERVAL 100   // Poll every 100ms

// JSON reporting configuration
#define JSON_REPORT_INTERVAL 500    // Send JSON to console every 500ms
#define JSON_REPORT_DURATION 5000   // Report for 5 seconds (10 messages)

// Machine operating modes
#define MODE_NONE     0       // No specific mode
#define MODE_SPINDLE  1       // Spindle mode (CNC)
#define MODE_LASER    2       // Laser mode
#define MODE_DRAWING  3       // Drawing/pen plotter mode

// Function declarations
void mks_i2c_slave_init();                    // Initialize I2C
void mks_i2c_poll_task(void* parameter);      // FreeRTOS task for polling I2C
void mks_i2c_process_json(const char* json);  // Process JSON commands
void report_machine_mode();                   // Report the current machine mode
const char* get_machine_mode_string();        // Get machine mode as string
void forward_json_to_console(const char* json); // Forward JSON to UGS console
bool send_json_to_arduino(const char* json);  // Send JSON to Arduino through I2C

// External vars
extern uint8_t mks_machine_mode;              // Current machine mode