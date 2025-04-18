#include "MKS_I2C_Slave.h"

// Global variables
uint8_t mks_machine_mode = MODE_NONE; // Default mode is none
TaskHandle_t i2cTaskHandle = NULL;    // Task handle for the polling task
char i2c_buffer[256];                 // Buffer for I2C data

// Initialize I2C functionality
void mks_i2c_slave_init() {
    // Initialize with default mode
    mks_machine_mode = MODE_NONE;
    
    // Initialize I2C hardware
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // Create a task for polling I2C
    xTaskCreate(
        mks_i2c_poll_task,    // Function that implements the task
        "i2cPollTask",        // Text name for the task
        4096,                 // Stack size in words
        NULL,                 // Parameter passed into the task
        1,                    // Priority (0 is lowest, configMAX_PRIORITIES-1 is highest)
        &i2cTaskHandle        // Task handle
    );
    
    // Log initialization
    grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "I2C initialized with polling on pins %d (SDA) and %d (SCL)", I2C_SDA_PIN, I2C_SCL_PIN);
}

// FreeRTOS task for periodically polling I2C
void mks_i2c_poll_task(void* parameter) {
    for (;;) {
        // Check if any JSON command is available via serial for testing
        if (Serial.available() > 0) {
            size_t len = Serial.readBytesUntil('\n', i2c_buffer, sizeof(i2c_buffer) - 1);
            if (len > 0) {
                i2c_buffer[len] = '\0';
                
                // Process commands that start with J: as JSON commands
                if (len > 2 && i2c_buffer[0] == 'J' && i2c_buffer[1] == ':') {
                    // Forward the raw JSON to UGS console
                    forward_json_to_console(&i2c_buffer[2]);
                    
                    // Process the command
                    mks_i2c_process_json(&i2c_buffer[2]);
                }
            }
        }
        
        // This task should run at a low priority and not interfere with G-code processing
        vTaskDelay(I2C_POLL_INTERVAL / portTICK_PERIOD_MS);
    }
}

// Forward JSON to UGS console
void forward_json_to_console(const char* json) {
    // Forward the raw JSON to the serial console for UGS to receive
    grbl_sendf(CLIENT_SERIAL, "[JSON:%s]\r\n", json);
}

// Process JSON commands
void mks_i2c_process_json(const char* json) {
    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Error, "JSON parse error: %s", error.c_str());
        return;
    }
    
    // Check for mode command
    if (doc.containsKey("mode")) {
        const char* mode = doc["mode"];
        if (mode) {
            if (strcmp(mode, "spindle") == 0) {
                mks_machine_mode = MODE_SPINDLE;
            } 
            else if (strcmp(mode, "laser") == 0) {
                mks_machine_mode = MODE_LASER;
            }
            else if (strcmp(mode, "drawing") == 0) {
                mks_machine_mode = MODE_DRAWING;
            }
            else if (strcmp(mode, "none") == 0) {
                mks_machine_mode = MODE_NONE;
            }
            
            // Report mode change
            report_machine_mode();
        }
    }
    
    // Check for gcode command
    if (doc.containsKey("gcode")) {
        const char* gcode = doc["gcode"];
        if (gcode) {
            // Execute the G-code command
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Debug, "Executing G-code: %s", gcode);
            
            // Prepare and execute the G-code
            char gc_line[256];
            strncpy(gc_line, gcode, 255);
            gc_line[255] = '\0';
            
            Error status_code = gc_execute_line(gc_line, CLIENT_SERIAL);
            
            // Handle errors
            if (status_code != Error::Ok) {
                grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Error, "G-code error: %s", errorString(status_code));
            }
        }
    }
    
    // Other commands can be handled here
}

// Get machine mode as a string
const char* get_machine_mode_string() {
    switch (mks_machine_mode) {
        case MODE_SPINDLE: return "spindle";
        case MODE_LASER: return "laser";
        case MODE_DRAWING: return "drawing";
        case MODE_NONE: 
        default:
            return "none";
    }
}

// Report the current machine mode - this gets called during startup after the settings are reported
void report_machine_mode() {
    grbl_sendf(CLIENT_ALL, "[MODE:%s]\r\n", get_machine_mode_string());
}