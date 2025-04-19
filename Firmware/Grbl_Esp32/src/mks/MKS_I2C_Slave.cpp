#include "MKS_I2C_Slave.h"

// Global variables
uint8_t mks_machine_mode = MODE_NONE; // Default mode is none
TaskHandle_t i2cTaskHandle = NULL;    // Task handle for the polling task
char i2c_buffer[256];                 // Buffer for I2C data

// Variables for throttled JSON reporting
uint32_t last_json_report_time = 0;   // Last time JSON was reported
uint32_t mode_change_time = 0;        // Time when mode change was detected
bool throttled_reporting_active = false; // Flag to indicate if throttled reporting is active
char last_json_content[256] = {0};    // Store the last JSON content for repeat sending

// Initialize I2C functionality
void mks_i2c_slave_init() {
    // Initialize with default mode
    mks_machine_mode = MODE_NONE;
    
    // Initialize I2C hardware with specified frequency
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_CLOCK_FREQ); // Set clock frequency to 100 kHz
    
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
    grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "I2C initialized with polling on pins %d (SDA) and %d (SCL) at %d Hz", 
                  I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_FREQ);
}

// FreeRTOS task for periodically polling I2C
void mks_i2c_poll_task(void* parameter) {
    for (;;) {
        uint32_t current_time = millis();
        
        // Handle throttled reporting if active
        if (throttled_reporting_active) {
            // Check if we should continue throttled reporting
            if ((current_time - mode_change_time) <= JSON_REPORT_DURATION) {
                // Check if it's time to send another report
                if ((current_time - last_json_report_time) >= JSON_REPORT_INTERVAL) {
                    // Send the stored JSON to console
                    grbl_sendf(CLIENT_SERIAL, "[JSON:%s]\r\n", last_json_content);
                    last_json_report_time = current_time;
                }
            } else {
                // Reporting duration expired
                throttled_reporting_active = false;
            }
        }

        // Check if any JSON command is available via serial for testing
        if (Serial.available() > 0) {
            size_t len = Serial.readBytesUntil('\n', i2c_buffer, sizeof(i2c_buffer) - 2);
            if (len > 0) {
                i2c_buffer[len] = '\0';
                
                // Process commands that start with J: as JSON commands
                if (len > 2 && i2c_buffer[0] == 'J' && i2c_buffer[1] == ':' && len > 3) {
                    // Store JSON for throttled reporting
                    strncpy(last_json_content, &i2c_buffer[2], sizeof(last_json_content) - 1);
                    last_json_content[sizeof(last_json_content) - 1] = '\0';
                    
                    // Start throttled reporting
                    if (!throttled_reporting_active) {
                        // Send first report immediately
                        forward_json_to_console(&i2c_buffer[2]);
                        mode_change_time = current_time;
                        last_json_report_time = current_time;
                        throttled_reporting_active = true;
                    }
                    
                    // Process the command regardless of throttling
                    mks_i2c_process_json(&i2c_buffer[2]);
                }
            }
        }
        
        // Poll Arduino Nano (slave at address 0x08) for JSON data
        Wire.requestFrom(0x08, 32); 
        if (Wire.available()) {
            size_t i = 0;
            while (Wire.available() && i < sizeof(i2c_buffer) - 2) {
                i2c_buffer[i++] = Wire.read();
            }
            i2c_buffer[i] = '\0';
            
            if (i > 0) {
                // Check if received data is valid JSON 
                if (i2c_buffer[0] == '{') {
                    // Store JSON for throttled reporting
                    strncpy(last_json_content, i2c_buffer, sizeof(last_json_content) - 1);
                    last_json_content[sizeof(last_json_content) - 1] = '\0';
                    
                    // Start throttled reporting if not already active
                    if (!throttled_reporting_active) {
                        // Send first report immediately
                        forward_json_to_console(i2c_buffer);
                        mode_change_time = current_time;
                        last_json_report_time = current_time;
                        throttled_reporting_active = true;
                    }
                    
                    // Process the JSON command regardless of throttling
                    mks_i2c_process_json(i2c_buffer);
                }
            }
        }
        
        // This task should run at a low priority and not interfere with G-code processing
        // Ensure a minimum delay to prevent excessive CPU usage
        const TickType_t minDelay = 10 / portTICK_PERIOD_MS; // Minimum delay of 10ms
        TickType_t delay = (I2C_POLL_INTERVAL / portTICK_PERIOD_MS) > minDelay ? 
                           (I2C_POLL_INTERVAL / portTICK_PERIOD_MS) : minDelay;
        vTaskDelay(delay);
    }
}

// Forward JSON to UGS console - now only used for the first immediate report
void forward_json_to_console(const char* json) {
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
            uint8_t previous_mode = mks_machine_mode;
            
            if (strncmp(mode, "spindle", 7) == 0) {
                mks_machine_mode = MODE_SPINDLE;
            }
            else if (strncmp(mode, "laser", 5) == 0) {
                mks_machine_mode = MODE_LASER;
            }
            else if (strncmp(mode, "drawing", 7) == 0) {
                mks_machine_mode = MODE_DRAWING;
            }
            else if (strncmp(mode, "none", 4) == 0) {
                mks_machine_mode = MODE_NONE;
            }
            
            // Report mode change to all clients
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