#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "../Grbl.h"

// I2C Configuration
#define I2C_SDA_PIN 21          // I2C SDA pin (modify as needed for your board)
#define I2C_SCL_PIN 22          // I2C SCL pin (modify as needed for your board)
#define I2C_CLOCK_FREQ 100000   // 100kHz I2C clock frequency
#define I2C_POLL_INTERVAL 100   // Poll every 100ms

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

// External vars
extern uint8_t mks_machine_mode;              // Current machine mode