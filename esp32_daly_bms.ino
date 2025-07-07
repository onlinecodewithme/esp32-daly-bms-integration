/*
 * ESP32 Daly Smart BMS Bluetooth Reader
 * Connects to Daly Smart BMS via Bluetooth Classic and reads battery data
 * BMS MAC Address: 41:18:12:01:18:9F
 */

#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// Daly BMS Configuration
const String BMS_MAC = "41:18:12:01:18:9F";
const String BMS_NAME = "DalyBMS";

// Daly BMS Command Structure
struct DalyCommand {
  uint8_t start = 0xA5;     // Start byte
  uint8_t host_addr = 0x80; // Host address (PC/ESP32)
  uint8_t bms_addr = 0x01;  // BMS address
  uint8_t data_len = 0x08;  // Data length
  uint8_t command = 0x90;   // Command ID
  uint8_t data[8] = {0};    // Data bytes (8 bytes)
  uint8_t checksum = 0;     // Checksum
};

// BMS Data Structure
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
};

BMSData bmsData;
bool connected = false;
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 5000; // Read every 5 seconds

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Daly BMS Bluetooth Reader Starting...");
  
  SerialBT.begin("ESP32_BMS_Reader"); // Bluetooth device name
  Serial.println("Bluetooth initialized. Attempting to connect to Daly BMS...");
  
  // Attempt to connect to BMS
  connectToBMS();
}

void loop() {
  if (!connected) {
    Serial.println("Attempting to reconnect to BMS...");
    connectToBMS();
    delay(5000);
    return;
  }
  
  // Check if it's time to read data
  if (millis() - lastReadTime >= READ_INTERVAL) {
    readBMSData();
    lastReadTime = millis();
  }
  
  // Check connection status
  if (!SerialBT.connected()) {
    Serial.println("BMS disconnected!");
    connected = false;
  }
  
  delay(100);
}

void connectToBMS() {
  Serial.println("Connecting to BMS: " + BMS_MAC);
  
  // Convert MAC address string to uint8_t array
  uint8_t mac[6];
  if (parseMacAddress(BMS_MAC, mac)) {
    connected = SerialBT.connect(mac);
    if (connected) {
      Serial.println("Successfully connected to Daly BMS!");
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

void readBMSData() {
  Serial.println("Reading BMS data...");
  
  // Read different data types from BMS
  readVoltageCurrentSOC();    // Command 0x90
  delay(100);
  readCellVoltages();         // Command 0x91
  delay(100);
  readTemperatures();         // Command 0x92
  delay(100);
  readBalanceStatus();        // Command 0x93
  delay(100);
  readProtectionStatus();     // Command 0x94
  delay(100);
  
  // Display collected data
  displayBMSData();
}

void readVoltageCurrentSOC() {
  DalyCommand cmd;
  cmd.command = 0x90; // Voltage, Current, SOC command
  sendCommand(cmd);
  
  uint8_t response[13];
  if (readResponse(response, 13)) {
    // Parse voltage (bytes 4-5)
    bmsData.voltage = ((response[4] << 8) | response[5]) * 0.1;
    
    // Parse current (bytes 8-9) - signed value
    int16_t current_raw = (response[8] << 8) | response[9];
    bmsData.current = current_raw * 0.1;
    
    // Parse SOC (bytes 10-11)
    bmsData.soc = ((response[10] << 8) | response[11]) * 0.1;
  }
}

void readCellVoltages() {
  DalyCommand cmd;
  cmd.command = 0x91; // Cell voltages command
  sendCommand(cmd);
  
  uint8_t response[13];
  if (readResponse(response, 13)) {
    // Parse max cell voltage (bytes 4-5)
    bmsData.max_cell_voltage = (response[4] << 8) | response[5];
    
    // Parse min cell voltage (bytes 8-9)
    bmsData.min_cell_voltage = (response[8] << 8) | response[9];
  }
}

void readTemperatures() {
  DalyCommand cmd;
  cmd.command = 0x92; // Temperature command
  sendCommand(cmd);
  
  uint8_t response[13];
  if (readResponse(response, 13)) {
    // Parse max temperature (byte 4) - offset by 40
    bmsData.max_temp = response[4] - 40;
    
    // Parse min temperature (byte 6) - offset by 40
    bmsData.min_temp = response[6] - 40;
  }
}

void readBalanceStatus() {
  DalyCommand cmd;
  cmd.command = 0x93; // Balance status command
  sendCommand(cmd);
  
  uint8_t response[13];
  readResponse(response, 13); // Read but don't process for now
}

void readProtectionStatus() {
  DalyCommand cmd;
  cmd.command = 0x94; // Protection status command
  sendCommand(cmd);
  
  uint8_t response[13];
  if (readResponse(response, 13)) {
    // Check protection status (byte 4)
    bmsData.protection_status = (response[4] != 0);
  }
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
}

uint8_t calculateChecksum(const DalyCommand& cmd) {
  uint8_t sum = cmd.start + cmd.host_addr + cmd.bms_addr + cmd.data_len + cmd.command;
  for (int i = 0; i < 8; i++) {
    sum += cmd.data[i];
  }
  return sum;
}

bool readResponse(uint8_t* buffer, int expectedLength) {
  unsigned long startTime = millis();
  int bytesRead = 0;
  
  while (bytesRead < expectedLength && (millis() - startTime) < 1000) {
    if (SerialBT.available()) {
      buffer[bytesRead] = SerialBT.read();
      bytesRead++;
    }
    delay(10);
  }
  
  if (bytesRead == expectedLength) {
    // Verify response format
    if (buffer[0] == 0xA5) { // Check start byte
      return true;
    }
  }
  
  Serial.println("Failed to read valid response");
  return false;
}

void displayBMSData() {
  Serial.println("=== BMS Data ===");
  Serial.println("Voltage: " + String(bmsData.voltage, 2) + " V");
  Serial.println("Current: " + String(bmsData.current, 2) + " A");
  Serial.println("SOC: " + String(bmsData.soc, 1) + " %");
  Serial.println("Max Cell Voltage: " + String(bmsData.max_cell_voltage) + " mV");
  Serial.println("Min Cell Voltage: " + String(bmsData.min_cell_voltage) + " mV");
  Serial.println("Max Temperature: " + String(bmsData.max_temp) + " °C");
  Serial.println("Min Temperature: " + String(bmsData.min_temp) + " °C");
  Serial.println("Protection Status: " + String(bmsData.protection_status ? "ACTIVE" : "NORMAL"));
  Serial.println("================");
  Serial.println();
}
