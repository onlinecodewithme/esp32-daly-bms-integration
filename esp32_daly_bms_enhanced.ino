/*
 * ESP32 Daly Smart BMS Bluetooth Reader - Enhanced Version
 * Connects to Daly Smart BMS via Bluetooth Classic and reads battery data
 * Features: Enhanced error handling, JSON output, detailed logging, reconnection logic
 * 
 * BMS MAC Address: 41:18:12:01:18:9F
 * 
 * Author: ESP32 BMS Integration Project
 * Version: 2.0
 */

#include "BluetoothSerial.h"
#include "config.h"

// Forward declaration of BMSData structure for utils.h
struct BMSData {
  float voltage = 0.0;           // Total voltage (V)
  float current = 0.0;           // Current (A)
  float soc = 0.0;               // State of charge (%)
  uint16_t max_cell_voltage = 0; // Max cell voltage (mV)
  uint16_t min_cell_voltage = 0; // Min cell voltage (mV)
  uint8_t max_temp = 0;          // Max temperature (°C)
  uint8_t min_temp = 0;          // Min temperature (°C)
  uint16_t cycles = 0;           // Charge cycles
  bool protection_status = false; // Protection status
  float remaining_capacity = 0.0; // Remaining capacity (Ah)
  float full_capacity = 0.0;     // Full capacity (Ah)
  bool data_valid = false;       // Data validity flag
  unsigned long last_update = 0; // Last successful update timestamp
};

#include "utils.h"

BluetoothSerial SerialBT;

// Daly BMS Command Structure
struct DalyCommand {
  uint8_t start = DALY_START_BYTE;     // Start byte
  uint8_t host_addr = DALY_HOST_ADDR;  // Host address (PC/ESP32)
  uint8_t bms_addr = DALY_BMS_ADDR;    // BMS address
  uint8_t data_len = DALY_DATA_LEN;    // Data length
  uint8_t command = CMD_VOUT_IOUT_SOC; // Command ID
  uint8_t data[8] = {0};               // Data bytes (8 bytes)
  uint8_t checksum = 0;                // Checksum
};

// Global variables
BMSData bmsData;
bool connected = false;
unsigned long lastReadTime = 0;
unsigned long lastConnectionAttempt = 0;
int reconnectAttempts = 0;
bool outputMode = 0; // 0 = Normal, 1 = JSON, 2 = CSV

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  
  Serial.println("=== ESP32 Daly BMS Bluetooth Reader v2.0 ===");
  Serial.println("Enhanced version with improved error handling");
  Serial.println("BMS MAC: " + String(BMS_MAC_ADDRESS));
  Serial.println("Read Interval: " + String(READ_INTERVAL / 1000) + " seconds");
  Serial.println("============================================");
  
  SerialBT.begin(ESP32_BT_NAME);
  Serial.println("Bluetooth initialized as: " + String(ESP32_BT_NAME));
  
  // Print available commands
  printCommands();
  
  // Attempt initial connection
  connectToBMS();
}

void loop() {
  // Handle serial commands
  handleSerialCommands();
  
  // Connection management
  if (!connected) {
    if (millis() - lastConnectionAttempt >= RECONNECT_DELAY) {
      if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        Serial.println("Reconnection attempt " + String(reconnectAttempts + 1) + 
                      "/" + String(MAX_RECONNECT_ATTEMPTS));
        connectToBMS();
        reconnectAttempts++;
        lastConnectionAttempt = millis();
      } else {
        Serial.println("Max reconnection attempts reached. Waiting...");
        delay(30000); // Wait 30 seconds before resetting attempts
        reconnectAttempts = 0;
      }
    }
    return;
  }
  
  // Check if it's time to read data
  if (millis() - lastReadTime >= READ_INTERVAL) {
    if (readBMSData()) {
      bmsData.last_update = millis();
      bmsData.data_valid = true;
      
      // Output data based on selected mode
      switch (outputMode) {
        case 0: // Normal output
          displayBMSData();
          break;
        case 1: // JSON output
          Serial.println(createJSONOutput(bmsData));
          break;
        case 2: // CSV output
          logCSVData(bmsData);
          break;
      }
    } else {
      bmsData.data_valid = false;
      Serial.println("Failed to read BMS data");
    }
    lastReadTime = millis();
  }
  
  // Check connection status
  if (!SerialBT.connected()) {
    Serial.println("BMS connection lost!");
    connected = false;
    reconnectAttempts = 0;
  }
  
  delay(100);
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "help" || command == "h") {
      printCommands();
    } else if (command == "status" || command == "s") {
      printSystemStatus();
    } else if (command == "detailed" || command == "d") {
      if (bmsData.data_valid) {
        printDetailedStatus(bmsData);
      } else {
        Serial.println("No valid BMS data available");
      }
    } else if (command == "json" || command == "j") {
      outputMode = 1;
      Serial.println("Output mode set to JSON");
    } else if (command == "csv" || command == "c") {
      outputMode = 2;
      Serial.println("Output mode set to CSV");
    } else if (command == "normal" || command == "n") {
      outputMode = 0;
      Serial.println("Output mode set to Normal");
    } else if (command == "reconnect" || command == "r") {
      Serial.println("Forcing reconnection...");
      SerialBT.disconnect();
      connected = false;
      reconnectAttempts = 0;
    } else if (command == "debug") {
      Serial.println("Debug info:");
      Serial.println("Connected: " + String(connected ? "Yes" : "No"));
      Serial.println("BT Connected: " + String(SerialBT.connected() ? "Yes" : "No"));
      Serial.println("Last read: " + String((millis() - lastReadTime) / 1000) + "s ago");
      Serial.println("Data valid: " + String(bmsData.data_valid ? "Yes" : "No"));
    } else if (command != "") {
      Serial.println("Unknown command: " + command);
      Serial.println("Type 'help' for available commands");
    }
  }
}

void printCommands() {
  Serial.println("\n=== Available Commands ===");
  Serial.println("help (h)     - Show this help");
  Serial.println("status (s)   - Show system status");
  Serial.println("detailed (d) - Show detailed BMS status");
  Serial.println("json (j)     - Switch to JSON output");
  Serial.println("csv (c)      - Switch to CSV output");
  Serial.println("normal (n)   - Switch to normal output");
  Serial.println("reconnect (r)- Force reconnection");
  Serial.println("debug        - Show debug information");
  Serial.println("==========================\n");
}

void printSystemStatus() {
  Serial.println("\n=== System Status ===");
  Serial.println("Uptime: " + formatUptime(millis()));
  Serial.println("BMS Connected: " + String(connected ? "Yes" : "No"));
  Serial.println("Bluetooth Status: " + String(SerialBT.connected() ? "Connected" : "Disconnected"));
  Serial.println("Output Mode: " + String(outputMode == 0 ? "Normal" : (outputMode == 1 ? "JSON" : "CSV")));
  Serial.println("Read Interval: " + String(READ_INTERVAL / 1000) + " seconds");
  Serial.println("Reconnect Attempts: " + String(reconnectAttempts) + "/" + String(MAX_RECONNECT_ATTEMPTS));
  
  if (bmsData.data_valid) {
    Serial.println("Last Data Update: " + String((millis() - bmsData.last_update) / 1000) + " seconds ago");
    Serial.println("Battery Status: " + getBatteryStatus(bmsData));
  } else {
    Serial.println("No valid BMS data");
  }
  Serial.println("====================\n");
}

void connectToBMS() {
  Serial.println("Connecting to BMS: " + String(BMS_MAC_ADDRESS));
  
  // Convert MAC address string to uint8_t array
  uint8_t mac[6];
  if (parseMacAddress(String(BMS_MAC_ADDRESS), mac)) {
    connected = SerialBT.connect(mac);
    if (connected) {
      Serial.println("Successfully connected to Daly BMS!");
      reconnectAttempts = 0;
      delay(1000); // Give time for connection to stabilize
    } else {
      Serial.println("Failed to connect to BMS");
    }
  } else {
    Serial.println("Invalid MAC address format");
  }
}

bool parseMacAddress(String macStr, uint8_t* mac) {
  // Remove colons and convert hex string to bytes
  macStr.replace(":", "");
  if (macStr.length() != 12) return false;
  
  for (int i = 0; i < 6; i++) {
    String byteStr = macStr.substring(i * 2, i * 2 + 2);
    mac[i] = strtol(byteStr.c_str(), NULL, 16);
  }
  return true;
}

bool readBMSData() {
  if (DEBUG_ENABLED) {
    Serial.println("Reading BMS data...");
  }
  
  bool success = true;
  
  // Read different data types from BMS
  success &= readVoltageCurrentSOC();    // Command 0x90
  delay(100);
  success &= readCellVoltages();         // Command 0x91
  delay(100);
  success &= readTemperatures();         // Command 0x92
  delay(100);
  success &= readProtectionStatus();     // Command 0x94
  delay(100);
  
  return success;
}

bool readVoltageCurrentSOC() {
  DalyCommand cmd;
  cmd.command = CMD_VOUT_IOUT_SOC;
  sendCommand(cmd);
  
  uint8_t response[13];
  if (readResponse(response, 13, CMD_VOUT_IOUT_SOC)) {
    // Parse voltage (bytes 4-5)
    bmsData.voltage = ((response[4] << 8) | response[5]) * VOLTAGE_SCALE;
    
    // Parse current (bytes 8-9) - signed value
    int16_t current_raw = (response[8] << 8) | response[9];
    bmsData.current = current_raw * CURRENT_SCALE;
    
    // Parse SOC (bytes 10-11)
    bmsData.soc = ((response[10] << 8) | response[11]) * SOC_SCALE;
    
    return true;
  }
  return false;
}

bool readCellVoltages() {
  DalyCommand cmd;
  cmd.command = CMD_MIN_MAX_CELL_VOLTAGE;
  sendCommand(cmd);
  
  uint8_t response[13];
  if (readResponse(response, 13, CMD_MIN_MAX_CELL_VOLTAGE)) {
    // Parse max cell voltage (bytes 4-5)
    bmsData.max_cell_voltage = ((response[4] << 8) | response[5]) * CELL_VOLTAGE_SCALE;
    
    // Parse min cell voltage (bytes 8-9)
    bmsData.min_cell_voltage = ((response[8] << 8) | response[9]) * CELL_VOLTAGE_SCALE;
    
    return true;
  }
  return false;
}

bool readTemperatures() {
  DalyCommand cmd;
  cmd.command = CMD_MIN_MAX_TEMPERATURE;
  sendCommand(cmd);
  
  uint8_t response[13];
  if (readResponse(response, 13, CMD_MIN_MAX_TEMPERATURE)) {
    // Parse max temperature (byte 4) - offset by 40
    bmsData.max_temp = response[4] - TEMPERATURE_OFFSET;
    
    // Parse min temperature (byte 6) - offset by 40
    bmsData.min_temp = response[6] - TEMPERATURE_OFFSET;
    
    return true;
  }
  return false;
}

bool readProtectionStatus() {
  DalyCommand cmd;
  cmd.command = CMD_STATUS_INFO;
  sendCommand(cmd);
  
  uint8_t response[13];
  if (readResponse(response, 13, CMD_STATUS_INFO)) {
    // Check protection status (byte 4)
    bmsData.protection_status = (response[4] != 0);
    return true;
  }
  return false;
}

void sendCommand(DalyCommand& cmd) {
  // Calculate checksum
  cmd.checksum = calculateChecksum(cmd);
  
  // Send command bytes
  SerialBT.write(cmd.start);
  SerialBT.write(cmd.host_addr);
  SerialBT.write(cmd.bms_addr);
  SerialBT.write(cmd.data_len);
  SerialBT.write(cmd.command);
  
  for (int i = 0; i < 8; i++) {
    SerialBT.write(cmd.data[i]);
  }
  
  SerialBT.write(cmd.checksum);
  SerialBT.flush();
  
  if (DEBUG_RAW_DATA) {
    uint8_t cmdBytes[13] = {cmd.start, cmd.host_addr, cmd.bms_addr, cmd.data_len, cmd.command,
                           cmd.data[0], cmd.data[1], cmd.data[2], cmd.data[3],
                           cmd.data[4], cmd.data[5], cmd.data[6], cmd.data[7], cmd.checksum};
    printHexData(cmdBytes, 13, "Sent Command");
  }
}

uint8_t calculateChecksum(const DalyCommand& cmd) {
  uint8_t sum = cmd.start + cmd.host_addr + cmd.bms_addr + cmd.data_len + cmd.command;
  for (int i = 0; i < 8; i++) {
    sum += cmd.data[i];
  }
  return sum;
}

bool readResponse(uint8_t* buffer, int expectedLength, uint8_t expectedCommand) {
  unsigned long startTime = millis();
  int bytesRead = 0;
  
  while (bytesRead < expectedLength && (millis() - startTime) < RESPONSE_TIMEOUT) {
    if (SerialBT.available()) {
      buffer[bytesRead] = SerialBT.read();
      bytesRead++;
    }
    delay(10);
  }
  
  if (bytesRead == expectedLength) {
    if (DEBUG_RAW_DATA) {
      printHexData(buffer, expectedLength, "Received Response");
    }
    
    // Validate response
    if (validateResponse(buffer, expectedLength, expectedCommand)) {
      if (verifyChecksum(buffer, expectedLength)) {
        return true;
      }
    }
  }
  
  if (DEBUG_ENABLED) {
    Serial.println("Failed to read valid response for command 0x" + String(expectedCommand, HEX));
    Serial.println("Bytes read: " + String(bytesRead) + "/" + String(expectedLength));
  }
  return false;
}

void displayBMSData() {
  Serial.println("=== BMS Data ===");
  Serial.println("Voltage: " + String(bmsData.voltage, 2) + " V");
  Serial.println("Current: " + String(bmsData.current, 2) + " A");
  Serial.println("Power: " + String(bmsData.voltage * bmsData.current, 2) + " W");
  Serial.println("SOC: " + String(bmsData.soc, 1) + " % (" + getSOCStatus(bmsData.soc) + ")");
  Serial.println("Max Cell Voltage: " + String(bmsData.max_cell_voltage) + " mV");
  Serial.println("Min Cell Voltage: " + String(bmsData.min_cell_voltage) + " mV");
  
  if (bmsData.max_cell_voltage > 0 && bmsData.min_cell_voltage > 0) {
    int diff = bmsData.max_cell_voltage - bmsData.min_cell_voltage;
    Serial.println("Cell Voltage Diff: " + String(diff) + " mV");
  }
  
  Serial.println("Max Temperature: " + String(bmsData.max_temp) + " °C (" + 
                getTemperatureStatus(bmsData.max_temp) + ")");
  Serial.println("Min Temperature: " + String(bmsData.min_temp) + " °C");
  Serial.println("Status: " + getBatteryStatus(bmsData));
  Serial.println("Protection: " + String(bmsData.protection_status ? "ACTIVE" : "NORMAL"));
  Serial.println("================");
  Serial.println();
}
