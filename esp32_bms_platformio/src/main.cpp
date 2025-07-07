/*
 * ESP32 Daly Smart BMS BLE Reader v4.0
 * Connects to Daly Smart BMS via Bluetooth Low Energy (BLE) and reads battery data
 * Features: BLE device scanning, automatic BMS discovery, GATT communication, JSON output
 * BMS MAC Address: 41:18:12:01:18:9F
 * BMS Name: DL-41181201189F
 */

#include "Arduino.h"
#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLEScan.h"
#include "BLEAdvertisedDevice.h"
#include "BLEClient.h"

// Daly BMS Configuration
const String TARGET_BMS_MAC = "41:18:12:01:18:9F";
const String TARGET_BMS_NAME = "DL-41181201189F";
String discovered_bms_mac = "";
String discovered_bms_name = "";
bool bms_found_by_scan = false;

// BLE Configuration
BLEScan* pBLEScan;
BLEClient* pClient;
BLERemoteService* pRemoteService;
BLERemoteCharacteristic* pRemoteCharacteristic;

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
unsigned long lastScanTime = 0;
int deviceCount = 0;
const unsigned long READ_INTERVAL = 5000; // Read every 5 seconds
const unsigned long SCAN_INTERVAL = 30000; // Scan every 30 seconds if not connected

// Response handling variables
String lastResponse = "";
bool responseReceived = false;
uint8_t expectedCommand = 0;
BLERemoteCharacteristic* pNotifyCharacteristic = nullptr;

// Enhanced connection management
int connectionAttempts = 0;
unsigned long lastConnectionAttempt = 0;
bool autoConnect = true;

// Daly BMS Protocol Constants (from Python reference)
const uint8_t HEAD_READ[2] = {0xD2, 0x03};
const uint8_t CMD_INFO[6] = {0x00, 0x00, 0x00, 0x3E, 0xD7, 0xB9};
const uint8_t MOS_INFO[6] = {0x00, 0x3E, 0x00, 0x09, 0xF7, 0xA3};

// BLE Scan Callback Class
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    deviceCount++;
    
    String deviceName = advertisedDevice.getName().c_str();
    String deviceAddress = advertisedDevice.getAddress().toString().c_str();
    
    // Always show all discovered devices
    Serial.println("Device #" + String(deviceCount) + ": " + deviceName + " [" + deviceAddress + "]");
    Serial.println("  RSSI: " + String(advertisedDevice.getRSSI()) + " dBm");
    
    if (advertisedDevice.haveServiceUUID()) {
      Serial.print("  Service UUID: ");
      Serial.println(advertisedDevice.getServiceUUID().toString().c_str());
    }
    
    // Check if this might be a Daly BMS
    if (deviceName.indexOf("Daly") >= 0 || 
        deviceName.indexOf("BMS") >= 0 || 
        deviceName.indexOf("DL-") >= 0 ||
        deviceName.indexOf("41181201189F") >= 0 ||
        deviceAddress.equalsIgnoreCase(TARGET_BMS_MAC) ||
        deviceName.equalsIgnoreCase(TARGET_BMS_NAME)) {
      
      Serial.println("*** Potential BMS device found! ***");
      Serial.println("Name: " + deviceName);
      Serial.println("MAC: " + deviceAddress);
      
      if (deviceAddress.equalsIgnoreCase(TARGET_BMS_MAC) || 
          deviceName.equalsIgnoreCase(TARGET_BMS_NAME)) {
        Serial.println("*** Target BMS found! ***");
        discovered_bms_mac = deviceAddress;
        discovered_bms_name = deviceName;
        bms_found_by_scan = true;
      } else if (discovered_bms_mac.length() == 0) {
        // Store first potential BMS if target not found
        discovered_bms_mac = deviceAddress;
        discovered_bms_name = deviceName;
        Serial.println("*** Stored as potential BMS ***");
      }
    }
    Serial.println("---");
  }
};

// Notification callback function
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  Serial.print("Notification received: ");
  for (int i = 0; i < length; i++) {
    if (pData[i] < 16) Serial.print("0");
    Serial.print(String(pData[i], HEX));
  }
  Serial.println();
  
  // Store response for processing
  lastResponse = "";
  for (int i = 0; i < length; i++) {
    if (pData[i] < 16) lastResponse += "0";
    lastResponse += String(pData[i], HEX);
  }
  responseReceived = true;
}

// Function declarations
void scanForBMS();
void connectToBMS();
void readBMSData();
void readBMSDataDirect();
void tryMultipleServices();
bool tryService02f00000();
bool tryServiceFFF0();
bool tryDirectReads();
bool tryAlternativeCommands();
bool tryProperDalyProtocol();
bool sendDalyCommand(BLERemoteCharacteristic* pWriteChar, uint8_t command);
bool sendDalyCommandAndWait(BLERemoteCharacteristic* pWriteChar, uint8_t command, unsigned long timeout);
bool sendCommandAndCheck(BLERemoteCharacteristic* pWriteChar, uint8_t* command, size_t length, const char* name);
void readDalyResponse(uint8_t command);
bool setupNotifications(BLERemoteCharacteristic* pNotifyChar);
bool setupNotificationsWithDescriptor(BLERemoteCharacteristic* pNotifyChar);
void parseDalyResponse(uint8_t command, String hexData);
uint8_t calculateChecksum(uint8_t* data, int length);
void parseBMSCharacteristic(String uuid, std::string value);
void handleSerialCommands();
void printAvailableCommands();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== ESP32 Daly BMS BLE Reader v4.1 ===");
  Serial.println("Enhanced with proper Daly protocol + fallback methods");
  Serial.println("Target BMS MAC: " + TARGET_BMS_MAC);
  Serial.println("Target BMS Name: " + TARGET_BMS_NAME);
  Serial.println("==========================================");
  
  // Initialize BLE
  BLEDevice::init("ESP32_BMS_Reader");
  Serial.println("BLE initialized successfully.");
  
  // Create BLE Scanner
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // Active scan uses more power but gets more info
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  printAvailableCommands();
  
  // Start with a BLE scan
  scanForBMS();
}

void loop() {
  // Handle serial commands
  handleSerialCommands();
  
  if (!connected) {
    // Auto-connect if BMS found but not connected (and auto-connect is enabled)
    if (autoConnect && discovered_bms_mac.length() > 0) {
      // Prevent too frequent connection attempts
      if (millis() - lastConnectionAttempt >= 10000) {
        connectToBMS();
        lastConnectionAttempt = millis();
      }
    }
    
    // Scan for BMS periodically if not connected
    if (millis() - lastScanTime >= SCAN_INTERVAL) {
      scanForBMS();
      lastScanTime = millis();
    }
    
    delay(1000);
    return;
  }
  
  // Check if it's time to read data
  if (millis() - lastReadTime >= READ_INTERVAL) {
    readBMSData();
    lastReadTime = millis();
  }
  
  // Check connection status
  if (pClient && !pClient->isConnected()) {
    Serial.println("BMS connection lost!");
    connected = false;
  }
  
  delay(100);
}

void scanForBMS() {
  Serial.println("\n=== Scanning for BLE devices ===");
  Serial.println("Scanning for 10 seconds...");
  
  deviceCount = 0;
  
  // Start BLE scan
  BLEScanResults foundDevices = pBLEScan->start(10, false);
  
  Serial.println("=== Scan completed ===");
  Serial.println("Total devices found: " + String(deviceCount));
  
  if (deviceCount == 0) {
    Serial.println("No BLE devices discovered.");
    Serial.println("This could mean:");
    Serial.println("- No BLE devices in range are advertising");
    Serial.println("- Devices are in sleep mode");
    Serial.println("- BLE devices are not discoverable");
  }
  
  if (discovered_bms_mac.length() > 0) {
    Serial.println("BMS device to try: " + discovered_bms_name + " [" + discovered_bms_mac + "]");
    if (bms_found_by_scan) {
      Serial.println("Target BMS found by scan!");
    }
  } else {
    Serial.println("No BMS devices found in this scan.");
  }
  
  pBLEScan->clearResults(); // Delete results from BLEScan buffer
  Serial.println("=====================================\n");
}

void connectToBMS() {
  if (discovered_bms_mac.length() == 0) {
    Serial.println("No BMS device to connect to.");
    return;
  }
  
  connectionAttempts++;
  Serial.printf("Connection attempt #%d to: %s [%s]\n", 
                connectionAttempts, discovered_bms_name.c_str(), discovered_bms_mac.c_str());
  
  // Clean up previous client if exists
  if (pClient) {
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
  }
  
  // Create BLE client
  pClient = BLEDevice::createClient();
  Serial.println("BLE client created.");
  
  // Connect to the BLE Server
  BLEAddress bmsAddress(discovered_bms_mac.c_str());
  
  Serial.println("Attempting BLE connection...");
  if (pClient->connect(bmsAddress)) {
    Serial.println("*** Successfully connected to BMS via BLE! ***");
    Serial.println("Connected to: " + discovered_bms_name + " [" + discovered_bms_mac + "]");
    
    // List available services
    Serial.println("Discovering services...");
    std::map<std::string, BLERemoteService*>* services = pClient->getServices();
    
    Serial.println("Available services:");
    for (auto& service : *services) {
      Serial.println("  Service UUID: " + String(service.first.c_str()));
      
      // Get characteristics for this service
      std::map<std::string, BLERemoteCharacteristic*>* characteristics = service.second->getCharacteristics();
      for (auto& characteristic : *characteristics) {
        Serial.println("    Characteristic UUID: " + String(characteristic.first.c_str()));
        Serial.print("    Properties: ");
        Serial.print(characteristic.second->canRead() ? "R" : "-");
        Serial.print(characteristic.second->canWrite() ? "W" : "-");
        Serial.print(characteristic.second->canNotify() ? "N" : "-");
        Serial.println();
      }
    }
    
    connected = true;
    connectionAttempts = 0; // Reset counter on success
    
  } else {
    Serial.printf("❌ BLE connection failed (attempt #%d)\n", connectionAttempts);
    connected = false;
    
    // After 5 failed attempts, suggest rescanning
    if (connectionAttempts >= 5) {
      Serial.println("💡 Too many failed attempts. Try 'scan' to refresh BMS discovery.");
      connectionAttempts = 0;
      discovered_bms_mac = ""; // Clear to force rescan
    }
  }
}

void readBMSData() {
  if (!connected || !pClient || !pClient->isConnected()) {
    Serial.println("Not connected to BMS");
    return;
  }
  
  Serial.println("Reading BMS data - trying multiple approaches...");
  
  // Try multiple services and approaches
  tryMultipleServices();
}

void tryMultipleServices() {
  Serial.println("{");
  Serial.println("  \"timestamp\": " + String(millis()) + ",");
  Serial.println("  \"device\": \"" + discovered_bms_name + "\",");
  Serial.println("  \"mac_address\": \"" + discovered_bms_mac + "\",");
  Serial.println("  \"daly_protocol\": {");
  
  bool dataFound = tryProperDalyProtocol();
  
  Serial.println("  },");
  Serial.println("  \"data_found\": " + String(dataFound ? "true" : "false") + ",");
  
  // Add parsed BMS values (now using the corrected Daly protocol data)
  Serial.println("  \"parsed_data\": {");
  Serial.println("    \"voltage_v\": " + String(bmsData.voltage, 2) + ",");
  Serial.println("    \"current_a\": " + String(bmsData.current, 2) + ",");
  Serial.println("    \"power_w\": " + String(bmsData.voltage * bmsData.current, 2) + ",");
  Serial.println("    \"soc_percent\": " + String(bmsData.soc, 1) + ",");
  Serial.println("    \"max_cell_voltage_mv\": " + String(bmsData.max_cell_voltage) + ",");
  Serial.println("    \"min_cell_voltage_mv\": " + String(bmsData.min_cell_voltage) + ",");
  Serial.println("    \"cell_count\": " + String(bmsData.max_cell_voltage > 0 ? 16 : 0) + ",");
  Serial.println("    \"max_temperature_c\": " + String(bmsData.max_temp) + ",");
  Serial.println("    \"min_temperature_c\": " + String(bmsData.min_temp) + ",");
  Serial.println("    \"cycles\": " + String(bmsData.cycles) + ",");
  Serial.println("    \"protection_status\": " + String(bmsData.protection_status ? "true" : "false") + ",");
  Serial.println("    \"remaining_capacity_ah\": " + String(bmsData.remaining_capacity, 2) + ",");
  Serial.println("    \"full_capacity_ah\": " + String(bmsData.full_capacity, 2));
  Serial.println("  }");
  Serial.println("}");
}

// NEW: Proper Daly protocol implementation based on Python reference
bool tryProperDalyProtocol() {
  // Find the fff0 service (standard Daly service)
  BLERemoteService* pService = nullptr;
  std::map<std::string, BLERemoteService*>* services = pClient->getServices();
  
  for (auto& service : *services) {
    if (service.first.find("fff0") != std::string::npos) {
      pService = service.second;
      break;
    }
  }
  
  if (!pService) {
    Serial.println("      \"status\": \"fff0_service_not_found\"");
    return false;
  }
  
  // Find characteristics
  BLERemoteCharacteristic* pRxChar = pService->getCharacteristic(BLEUUID("fff1"));
  BLERemoteCharacteristic* pTxChar = pService->getCharacteristic(BLEUUID("fff2"));
  
  if (!pRxChar || !pTxChar) {
    Serial.println("      \"status\": \"required_characteristics_not_found\"");
    return false;
  }
  
  Serial.println("      \"status\": \"characteristics_found\",");
  
  // Setup notifications on RX characteristic
  if (pRxChar->canNotify()) {
    pRxChar->registerForNotify(notifyCallback);
    
    // Enable notifications via descriptor
    BLERemoteDescriptor* pDescriptor = pRxChar->getDescriptor(BLEUUID((uint16_t)0x2902));
    if (pDescriptor) {
      uint8_t notificationOn[] = {0x01, 0x00};
      pDescriptor->writeValue(notificationOn, 2, true);
      Serial.println("      \"notifications\": \"enabled\",");
    }
  }
  
  bool success = false;
  
  // Send main info command (CMD_INFO) using proper Daly protocol
  Serial.println("      \"commands\": {");
  Serial.println("        \"main_info\": {");
  
  // Prepare command: HEAD_READ + CMD_INFO
  uint8_t command[8];
  memcpy(command, HEAD_READ, 2);
  memcpy(command + 2, CMD_INFO, 6);
  
  Serial.print("          \"command_sent\": \"");
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X", command[i]);
  }
  Serial.println("\",");
  
  try {
    // Send command
    pTxChar->writeValue(command, 8);
    
    // Wait for response
    responseReceived = false;
    unsigned long startTime = millis();
    while (!responseReceived && (millis() - startTime < 3000)) {
      delay(10);
    }
    
    if (responseReceived) {
      Serial.println("          \"response_received\": true,");
      Serial.println("          \"response_data\": \"" + lastResponse + "\"");
      
      // Parse the response using proper Daly protocol
      if (lastResponse.length() >= 16) { // Minimum expected response length
        // Convert hex string to bytes for parsing
        int dataLen = lastResponse.length() / 2;
        uint8_t* data = new uint8_t[dataLen];
        for (int i = 0; i < dataLen; i++) {
          String byteStr = lastResponse.substring(i * 2, i * 2 + 2);
          data[i] = strtol(byteStr.c_str(), NULL, 16);
        }
        
        // Parse according to Daly protocol specification
        if (dataLen >= 124 && data[0] == 0xD2 && data[1] == 0x03) {
          // Main info response - parse according to actual Daly BLE format
          const int HEAD_LEN = 3;
          
          Serial.println(",");
          Serial.println("          \"parsing\": {");
          
          // Parse cell voltages (starting at offset 3, each cell is 2 bytes)
          // Cell voltages: 0cf6 = 3318mV, 0cf5 = 3317mV, etc.
          uint16_t totalVoltage = 0;
          uint16_t maxCellVoltage = 0;
          uint16_t minCellVoltage = 65535;
          int cellCount = 0;
          
          // Parse first 16 cells (32 bytes starting at offset 3)
          for (int i = 0; i < 16 && (HEAD_LEN + i*2 + 1) < dataLen; i++) {
            uint16_t cellVoltage = (data[HEAD_LEN + i*2] << 8) | data[HEAD_LEN + i*2 + 1];
            if (cellVoltage > 0x0A00 && cellVoltage < 0x1200) { // Valid cell voltage range (2.56V - 4.6V)
              totalVoltage += cellVoltage;
              cellCount++;
              if (cellVoltage > maxCellVoltage) maxCellVoltage = cellVoltage;
              if (cellVoltage < minCellVoltage) minCellVoltage = cellVoltage;
            }
          }
          
          if (cellCount > 0) {
            bmsData.voltage = totalVoltage / 1000.0; // Convert mV to V
            bmsData.max_cell_voltage = maxCellVoltage;
            bmsData.min_cell_voltage = minCellVoltage;
          }
          
          // Parse SOC - Based on response analysis, SOC appears to be at specific offset
          // Response pattern: d2037c + cell_voltages(32bytes) + ... + 0212 + 75 + 30 + 0388 + ...
          // The 0212 (530 decimal) could be SOC in 0.1% units = 53.0%
          if (dataLen > 70) {
            uint16_t socRaw = (data[68] << 8) | data[69]; // Around offset 68-69
            if (socRaw >= 0x0100 && socRaw <= 0x03E8) { // SOC range 25.6-100.0% in 0.1% units
              bmsData.soc = socRaw / 10.0;
            }
          }
          
          // Parse temperature - The 75 (0x4B) in response could be temperature
          // Daly format: temp = raw_value - 40, so 75-40 = 35°C (reasonable)
          if (dataLen > 69) {
            uint8_t tempRaw = data[69]; // Temperature byte after SOC
            if (tempRaw >= 40 && tempRaw <= 120) { // Raw range 40-120 = -0°C to 80°C
              int8_t temp = tempRaw - 40; // Daly offset
              bmsData.max_temp = temp;
              bmsData.min_temp = temp;
            }
          }
          
          // Parse current - Look for current around 30000 offset (0x7530)
          // In response: 7530 appears at offset 70-71, which is 30000 decimal
          if (dataLen > 72) {
            uint16_t currentRaw = (data[70] << 8) | data[71];
            if (currentRaw >= 29000 && currentRaw <= 31000) { // Around 30000 offset
              bmsData.current = (currentRaw - 30000) / 10.0; // 0.1A resolution
            }
          }
          
          // Parse cycles - Look for cycle count in reasonable range
          // The 0388 (904 decimal) could be cycles, but let's look for a more reasonable value
          // Check multiple positions for cycle data
          for (int i = 80; i < dataLen - 1; i++) {
            uint16_t cycles = (data[i] << 8) | data[i + 1];
            if (cycles > 0 && cycles < 5000) { // Reasonable cycle range
              bmsData.cycles = cycles;
              break;
            }
          }
          
          // If cycles not found in main area, check the end area
          if (bmsData.cycles == 0 && dataLen > 110) {
            // Look in the latter part of the response
            for (int i = 100; i < dataLen - 1; i++) {
              uint16_t cycles = (data[i] << 8) | data[i + 1];
              if (cycles > 0 && cycles < 10000) {
                bmsData.cycles = cycles;
                break;
              }
            }
          }
          
          Serial.println("            \"cell_count\": " + String(cellCount) + ",");
          Serial.println("            \"total_voltage_v\": " + String(bmsData.voltage, 2) + ",");
          Serial.println("            \"max_cell_mv\": " + String(bmsData.max_cell_voltage) + ",");
          Serial.println("            \"min_cell_mv\": " + String(bmsData.min_cell_voltage) + ",");
          Serial.println("            \"soc_percent\": " + String(bmsData.soc, 1) + ",");
          Serial.println("            \"current_a\": " + String(bmsData.current, 2) + ",");
          Serial.println("            \"temperature_c\": " + String(bmsData.max_temp, 1) + ",");
          Serial.println("            \"cycles\": " + String(bmsData.cycles));
          Serial.println("          }");
          
          success = true;
        }
        
        delete[] data;
      }
      
      responseReceived = false;
    } else {
      Serial.println("          \"response_received\": false");
    }
  } catch (const std::exception& e) {
    Serial.println("          \"error\": \"command_send_failed\"");
  }
  
  Serial.println("        }");
  Serial.println("      }");
  
  return success;
}

bool tryService02f00000() {
  BLERemoteService* pBMSService = nullptr;
  std::map<std::string, BLERemoteService*>* services = pClient->getServices();
  
  for (auto& service : *services) {
    if (service.first.find("02f00000") != std::string::npos) {
      pBMSService = service.second;
      break;
    }
  }
  
  if (!pBMSService) {
    Serial.println("      \"status\": \"service_not_found\"");
    return false;
  }
  
  Serial.println("      \"status\": \"service_found\",");
  
  // Find characteristics
  BLERemoteCharacteristic* pWriteChar = nullptr;
  BLERemoteCharacteristic* pNotifyChar = nullptr;
  
  std::map<std::string, BLERemoteCharacteristic*>* characteristics = pBMSService->getCharacteristics();
  
  for (auto& characteristic : *characteristics) {
    String uuid = String(characteristic.first.c_str());
    if (uuid.indexOf("ff01") >= 0 && characteristic.second->canWrite()) {
      pWriteChar = characteristic.second;
    }
    if (uuid.indexOf("ff02") >= 0 && characteristic.second->canNotify()) {
      pNotifyChar = characteristic.second;
    }
  }
  
  if (!pWriteChar || !pNotifyChar) {
    Serial.println("      \"characteristics\": \"missing\"");
    return false;
  }
  
  Serial.println("      \"characteristics\": \"found\",");
  
  // Setup notifications with descriptor
  if (setupNotificationsWithDescriptor(pNotifyChar)) {
    Serial.println("      \"notifications\": \"enabled\",");
    
    // Try sending commands
    Serial.println("      \"commands\": {");
    bool success = false;
    
    // Try command 0x90
    if (sendDalyCommandAndWait(pWriteChar, 0x90, 3000)) {
      Serial.println("        \"cmd_90\": \"success\",");
      success = true;
    } else {
      Serial.println("        \"cmd_90\": \"timeout\",");
    }
    
    // Try command 0x96 (temperature - might be more responsive)
    if (sendDalyCommandAndWait(pWriteChar, 0x96, 3000)) {
      Serial.println("        \"cmd_96\": \"success\"");
      success = true;
    } else {
      Serial.println("        \"cmd_96\": \"timeout\"");
    }
    
    Serial.println("      }");
    return success;
  } else {
    Serial.println("      \"notifications\": \"failed\"");
    return false;
  }
}

bool tryServiceFFF0() {
  BLERemoteService* pService = nullptr;
  std::map<std::string, BLERemoteService*>* services = pClient->getServices();
  
  for (auto& service : *services) {
    if (service.first.find("fff0") != std::string::npos) {
      pService = service.second;
      break;
    }
  }
  
  if (!pService) {
    Serial.println("      \"status\": \"service_not_found\"");
    return false;
  }
  
  Serial.println("      \"status\": \"service_found\",");
  Serial.println("      \"data\": {");
  
  // Try to read all characteristics in this service
  std::map<std::string, BLERemoteCharacteristic*>* characteristics = pService->getCharacteristics();
  bool dataFound = false;
  bool firstChar = true;
  
  for (auto& characteristic : *characteristics) {
    if (characteristic.second->canRead()) {
      try {
        if (!firstChar) Serial.println(",");
        firstChar = false;
        
        std::string value = characteristic.second->readValue();
        String charUUID = String(characteristic.first.c_str());
        
        Serial.print("        \"" + charUUID + "\": {");
        Serial.print("\"hex\": \"");
        
        for (int i = 0; i < value.length(); i++) {
          if ((uint8_t)value[i] < 16) Serial.print("0");
          Serial.print(String((uint8_t)value[i], HEX));
        }
        Serial.print("\", \"length\": " + String(value.length()) + "}");
        
        if (value.length() > 0) {
          dataFound = true;
          // Try to parse this data
          parseBMSCharacteristic(charUUID, value);
        }
        
      } catch (const std::exception& e) {
        Serial.print("        \"" + String(characteristic.first.c_str()) + "\": {\"error\": \"read_failed\"}");
      }
    }
  }
  
  Serial.println();
  Serial.println("      }");
  return dataFound;
}

bool tryDirectReads() {
  // Try reading from all readable characteristics across all services
  std::map<std::string, BLERemoteService*>* services = pClient->getServices();
  bool dataFound = false;
  bool firstService = true;
  
  for (auto& service : *services) {
    std::map<std::string, BLERemoteCharacteristic*>* characteristics = service.second->getCharacteristics();
    
    for (auto& characteristic : *characteristics) {
      if (characteristic.second->canRead()) {
        try {
          std::string value = characteristic.second->readValue();
          if (value.length() > 0) {
            if (!firstService) Serial.println(",");
            firstService = false;
            
            String serviceUUID = String(service.first.c_str());
            String charUUID = String(characteristic.first.c_str());
            
            Serial.print("        \"" + serviceUUID + "_" + charUUID + "\": \"");
            for (int i = 0; i < value.length(); i++) {
              if ((uint8_t)value[i] < 16) Serial.print("0");
              Serial.print(String((uint8_t)value[i], HEX));
            }
            Serial.print("\"");
            
            dataFound = true;
            
            // Try to parse meaningful data
            if (value.length() >= 4) {
              parseBMSCharacteristic(charUUID, value);
            }
          }
        } catch (const std::exception& e) {
          // Skip failed reads
        }
      }
    }
  }
  
  if (!dataFound) {
    Serial.print("        \"status\": \"no_readable_data\"");
  }
  
  return dataFound;
}

bool tryAlternativeCommands() {
  // Find any writable characteristic
  BLERemoteCharacteristic* pWriteChar = nullptr;
  std::map<std::string, BLERemoteService*>* services = pClient->getServices();
  
  for (auto& service : *services) {
    std::map<std::string, BLERemoteCharacteristic*>* characteristics = service.second->getCharacteristics();
    for (auto& characteristic : *characteristics) {
      if (characteristic.second->canWrite()) {
        pWriteChar = characteristic.second;
        break;
      }
    }
    if (pWriteChar) break;
  }
  
  if (!pWriteChar) {
    Serial.println("      \"status\": \"no_writable_characteristic\"");
    return false;
  }
  
  Serial.println("      \"status\": \"trying_alternative_formats\",");
  Serial.println("      \"attempts\": {");
  
  bool success = false;
  
  // Try 1: Simple ping
  uint8_t ping[] = {0x00};
  if (sendCommandAndCheck(pWriteChar, ping, sizeof(ping), "ping")) {
    success = true;
  }
  
  // Try 2: Alternative Daly format
  uint8_t alt_cmd[] = {0xaa, 0x80, 0x90, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8};
  if (sendCommandAndCheck(pWriteChar, alt_cmd, sizeof(alt_cmd), "alt_daly")) {
    success = true;
  }
  
  // Try 3: Short command
  uint8_t short_cmd[] = {0xa5, 0x90, 0x08, 0x00};
  if (sendCommandAndCheck(pWriteChar, short_cmd, sizeof(short_cmd), "short_cmd")) {
    success = true;
  }
  
  Serial.println("      }");
  return success;
}

void parseBMSCharacteristic(String uuid, std::string value) {
  // Enhanced BMS data parsing for Daly BLE protocol
  uint8_t* data = (uint8_t*)value.data();
  int len = value.length();
  
  Serial.print(", \"parsed\": {");
  
  // Parse based on characteristic UUID and data patterns
  if (uuid.indexOf("ff03") >= 0 && len >= 2) {
    // ff03 characteristic - could be voltage/status
    uint16_t val = (data[0] << 8) | data[1];
    if (val > 0) {
      // Try different interpretations
      float voltage = val * 0.01; // 0.01V resolution
      if (voltage >= 10.0 && voltage <= 60.0) {
        bmsData.voltage = voltage;
        Serial.print("\"voltage_v\": " + String(bmsData.voltage, 2));
      } else {
        voltage = val * 0.1; // 0.1V resolution
        if (voltage >= 10.0 && voltage <= 60.0) {
          bmsData.voltage = voltage;
          Serial.print("\"voltage_v\": " + String(bmsData.voltage, 1));
        }
      }
    }
  }
  
  if (uuid.indexOf("ff05") >= 0 && len >= 2) {
    // ff05 characteristic - could be current/SOC
    uint16_t val = (data[0] << 8) | data[1];
    if (val > 0) {
      // Try as SOC (0-100%)
      if (val <= 100) {
        bmsData.soc = val;
        Serial.print("\"soc_percent\": " + String(bmsData.soc, 0));
      } else if (val <= 1000) {
        bmsData.soc = val * 0.1;
        Serial.print("\"soc_percent\": " + String(bmsData.soc, 1));
      }
    }
  }
  
  // Parse standard BLE characteristics that might contain BMS data
  if (uuid.indexOf("2a04") >= 0 && len >= 8) {
    // Connection parameters characteristic - sometimes contains BMS data
    // Format: 08000a0000009001
    if (len >= 8) {
      uint16_t val1 = (data[0] << 8) | data[1]; // 0x0800 = 2048
      uint16_t val2 = (data[2] << 8) | data[3]; // 0x000a = 10
      uint16_t val3 = (data[4] << 8) | data[5]; // 0x0000 = 0
      uint16_t val4 = (data[6] << 8) | data[7]; // 0x9001 = 36865
      
      // Try interpreting as BMS data
      if (val1 > 100 && val1 < 6000) { // Voltage in 0.01V units
        float voltage = val1 * 0.01;
        if (voltage >= 10.0 && voltage <= 60.0) {
          bmsData.voltage = voltage;
          Serial.print("\"voltage_v\": " + String(bmsData.voltage, 2));
        }
      }
      
      if (val2 <= 100) { // SOC percentage
        bmsData.soc = val2;
        Serial.print(", \"soc_percent\": " + String(bmsData.soc, 0));
      }
      
      if (val4 > 2000 && val4 < 5000) { // Cell voltage in mV
        bmsData.max_cell_voltage = val4;
        bmsData.min_cell_voltage = val4;
        Serial.print(", \"cell_voltage_mv\": " + String(val4));
      }
    }
  }
  
  // Try to decode any multi-byte data as potential BMS values
  if (len >= 4) {
    for (int i = 0; i <= len - 4; i += 2) {
      uint16_t val = (data[i] << 8) | data[i + 1];
      
      // Check for voltage pattern (10-60V in various units)
      if (val >= 1000 && val <= 6000) { // 0.01V units
        float voltage = val * 0.01;
        if (voltage >= 10.0 && voltage <= 60.0 && bmsData.voltage == 0.0) {
          bmsData.voltage = voltage;
          Serial.print(", \"potential_voltage_v\": " + String(voltage, 2));
        }
      }
      
      // Check for cell voltage pattern (2.5-4.5V in mV)
      if (val >= 2500 && val <= 4500) {
        if (bmsData.max_cell_voltage == 0 || val > bmsData.max_cell_voltage) {
          bmsData.max_cell_voltage = val;
        }
        if (bmsData.min_cell_voltage == 0 || val < bmsData.min_cell_voltage) {
          bmsData.min_cell_voltage = val;
        }
        Serial.print(", \"potential_cell_mv\": " + String(val));
      }
      
      // Check for SOC pattern (0-100%)
      if (val <= 100 && bmsData.soc == 0.0) {
        bmsData.soc = val;
        Serial.print(", \"potential_soc\": " + String(val));
      }
    }
  }
  
  // Check for temperature pattern
  if (len >= 1) {
    uint8_t temp = data[0];
    if (temp >= 0 && temp <= 200) {
      int8_t actualTemp = temp - 40; // Common offset
      if (actualTemp >= -40 && actualTemp <= 85) {
        Serial.print(", \"temperature_c\": " + String(actualTemp));
        if (actualTemp > bmsData.max_temp) bmsData.max_temp = actualTemp;
        if (bmsData.min_temp == 0 || actualTemp < bmsData.min_temp) {
          bmsData.min_temp = actualTemp;
        }
      }
    }
  }
  
  Serial.print("}");
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "scan" || command == "s") {
      scanForBMS();
    } else if (command == "connect" || command == "c") {
      if (discovered_bms_mac.length() > 0) {
        Serial.println("Manual connection requested...");
        connectToBMS();
      } else {
        Serial.println("No BMS discovered. Run 'scan' first.");
      }
    } else if (command == "data" || command == "d") {
      if (connected) {
        readBMSData();
      } else {
        Serial.println("Not connected. Try 'scan' and 'connect' first.");
      }
    } else if (command == "status") {
      Serial.println("\n=== System Status ===");
      Serial.printf("Connected: %s\n", connected ? "✅ YES" : "❌ NO");
      Serial.printf("BMS Found: %s\n", discovered_bms_mac.length() > 0 ? "✅ YES" : "❌ NO");
      Serial.printf("Connection Attempts: %d\n", connectionAttempts);
      Serial.printf("Auto Connect: %s\n", autoConnect ? "✅ ON" : "❌ OFF");
      if (discovered_bms_mac.length() > 0) {
        Serial.printf("BMS: %s [%s]\n", discovered_bms_name.c_str(), discovered_bms_mac.c_str());
      }
      Serial.println("====================\n");
    } else if (command == "auto") {
      autoConnect = !autoConnect;
      Serial.printf("Auto-connect: %s\n", autoConnect ? "✅ ENABLED" : "❌ DISABLED");
    } else if (command == "help" || command == "h") {
      printAvailableCommands();
    } else if (command == "reset" || command == "r") {
      Serial.println("Resetting discovered BMS...");
      discovered_bms_mac = "";
      discovered_bms_name = "";
      bms_found_by_scan = false;
      connected = false;
      if (pClient && pClient->isConnected()) {
        pClient->disconnect();
      }
    } else if (command == "services" || command == "srv") {
      if (connected && pClient && pClient->isConnected()) {
        Serial.println("Listing BLE services and characteristics...");
        std::map<std::string, BLERemoteService*>* services = pClient->getServices();
        
        for (auto& service : *services) {
          Serial.println("Service: " + String(service.first.c_str()));
          
          std::map<std::string, BLERemoteCharacteristic*>* characteristics = service.second->getCharacteristics();
          for (auto& characteristic : *characteristics) {
            Serial.print("  Char: " + String(characteristic.first.c_str()) + " (Props: ");
            Serial.print(characteristic.second->canRead() ? "R" : "-");
            Serial.print(characteristic.second->canWrite() ? "W" : "-");
            Serial.println(")");
          }
        }
      } else {
        Serial.println("Not connected to BMS");
      }
    } else if (command != "") {
      Serial.println("❌ Unknown: " + command + ". Type 'help' for commands.");
    }
  }
}

void printAvailableCommands() {
  Serial.println("\n=== Commands ===");
  Serial.println("scan     - Scan for BMS devices");
  Serial.println("connect  - Manual connect to BMS");
  Serial.println("data     - Read BMS data (JSON)");
  Serial.println("status   - Show system status");
  Serial.println("auto     - Toggle auto-connect");
  Serial.println("reset    - Reset and disconnect");
  Serial.println("services - List BLE services/characteristics");
  Serial.println("help     - Show this help");
  Serial.println("================\n");
}

bool sendDalyCommand(BLERemoteCharacteristic* pWriteChar, uint8_t command) {
  // Format Daly BMS command according to H2.1_103E_309F specifications:
  // Frame Header + Communication Module Address + Data ID + Data Length + Data Content + Checksum
  // A5 80 [command] 08 00000000000000000000 [checksum]
  uint8_t message[13];
  message[0] = 0xA5;  // Frame Header
  message[1] = 0x80;  // Communication Module Address (PC to BMS)
  message[2] = command; // Data ID (Command)
  message[3] = 0x08;  // Data Length (8 bytes)
  
  // Data Content (8 bytes) - all zeros for read commands
  for (int i = 4; i < 12; i++) {
    message[i] = 0x00;
  }
  
  // Calculate checksum (sum of all bytes except checksum)
  message[12] = calculateChecksum(message, 12);
  
  Serial.print("Sending Daly command 0x");
  if (command < 16) Serial.print("0");
  Serial.print(String(command, HEX));
  Serial.print(": ");
  for (int i = 0; i < 13; i++) {
    if (message[i] < 16) Serial.print("0");
    Serial.print(String(message[i], HEX));
    if (i < 12) Serial.print(" ");
  }
  Serial.println();
  
  try {
    pWriteChar->writeValue(message, 13);
    delay(50); // Small delay to ensure command is sent
    return true;
  } catch (const std::exception& e) {
    Serial.println("Failed to send command");
    return false;
  }
}

bool sendDalyCommandAndWait(BLERemoteCharacteristic* pWriteChar, uint8_t command, unsigned long timeout) {
  if (!sendDalyCommand(pWriteChar, command)) {
    return false;
  }
  
  responseReceived = false;
  expectedCommand = command;
  
  unsigned long startTime = millis();
  while (!responseReceived && (millis() - startTime < timeout)) {
    delay(10);
  }
  
  if (responseReceived) {
    parseDalyResponse(command, lastResponse);
    responseReceived = false;
    return true;
  }
  
  return false;
}

bool sendCommandAndCheck(BLERemoteCharacteristic* pWriteChar, uint8_t* command, size_t length, const char* name) {
  Serial.print("        \"" + String(name) + "\": ");
  
  try {
    pWriteChar->writeValue(command, length);
    
    // Wait briefly for any response
    responseReceived = false;
    unsigned long startTime = millis();
    while (!responseReceived && (millis() - startTime < 1000)) {
      delay(10);
    }
    
    if (responseReceived) {
      Serial.println("\"response_received\",");
      responseReceived = false;
      return true;
    } else {
      Serial.println("\"no_response\",");
      return false;
    }
  } catch (const std::exception& e) {
    Serial.println("\"write_failed\",");
    return false;
  }
}

bool setupNotifications(BLERemoteCharacteristic* pNotifyChar) {
  if (!pNotifyChar) return false;
  
  try {
    pNotifyChar->registerForNotify(notifyCallback);
    return true;
  } catch (const std::exception& e) {
    Serial.println("Failed to setup notifications");
    return false;
  }
}

bool setupNotificationsWithDescriptor(BLERemoteCharacteristic* pNotifyChar) {
  if (!pNotifyChar) return false;
  
  try {
    // First register for notifications
    pNotifyChar->registerForNotify(notifyCallback);
    
    // Try to enable notifications via descriptor
    BLERemoteDescriptor* pDescriptor = pNotifyChar->getDescriptor(BLEUUID((uint16_t)0x2902));
    if (pDescriptor) {
      uint8_t notificationOn[] = {0x01, 0x00};
      pDescriptor->writeValue(notificationOn, 2, true);
    }
    
    delay(100); // Give time for setup
    return true;
  } catch (const std::exception& e) {
    Serial.println("Failed to setup notifications with descriptor");
    return false;
  }
}

void parseDalyResponse(uint8_t command, String hexData) {
  Serial.print("\"response_data\": {");
  Serial.print("\"raw_hex\": \"" + hexData + "\", ");
  
  // Convert hex string to bytes
  int dataLen = hexData.length() / 2;
  if (dataLen < 13) {
    Serial.print("\"error\": \"Response too short, expected 13 bytes, got " + String(dataLen) + "\"");
    Serial.print("}");
    return;
  }
  
  uint8_t* data = new uint8_t[dataLen];
  for (int i = 0; i < dataLen; i++) {
    String byteStr = hexData.substring(i * 2, i * 2 + 2);
    data[i] = strtol(byteStr.c_str(), NULL, 16);
  }
  
  // Verify Daly response format: A5 40 [command] 08 [data] [checksum]
  if (data[0] != 0xA5) {
    Serial.print("\"error\": \"Invalid header, expected A5, got " + String(data[0], HEX) + "\"");
    Serial.print("}");
    delete[] data;
    return;
  }
  
  if (data[1] != 0x40) {
    Serial.print("\"warning\": \"Unexpected address, expected 40 (BMS to PC), got " + String(data[1], HEX) + "\", ");
  }
  
  if (data[2] != command) {
    Serial.print("\"warning\": \"Command mismatch, expected " + String(command, HEX) + ", got " + String(data[2], HEX) + "\", ");
  }
  
  // Parse based on command type according to Daly H2.1_103E_309F specifications
  switch (command) {
    case 0x90: // SOC/Voltage/Current - Most important command
      {
        // Data format: [voltage_high][voltage_low][current_high][current_low][soc_high][soc_low][remaining][remaining]
        uint16_t voltage_raw = (data[4] << 8) | data[5];    // 0.1V resolution
        uint16_t current_raw = (data[6] << 8) | data[7];    // 0.1A resolution with 30000 offset
        uint16_t soc_raw = (data[8] << 8) | data[9];        // 0.1% resolution
        
        bmsData.voltage = voltage_raw * 0.1;
        bmsData.current = (current_raw - 30000) * 0.1;  // 30000 = 0A offset
        bmsData.soc = soc_raw * 0.1;
        
        Serial.print("\"voltage_v\": " + String(bmsData.voltage, 2) + ", ");
        Serial.print("\"current_a\": " + String(bmsData.current, 2) + ", ");
        Serial.print("\"soc_percent\": " + String(bmsData.soc, 1) + ", ");
        Serial.print("\"power_w\": " + String(bmsData.voltage * bmsData.current, 2));
      }
      break;
      
    case 0x91: // Cell Voltage Range
      {
        uint16_t max_voltage = (data[4] << 8) | data[5];    // mV
        uint8_t max_cell_num = data[6];
        uint16_t min_voltage = (data[7] << 8) | data[8];    // mV
        uint8_t min_cell_num = data[9];
        
        bmsData.max_cell_voltage = max_voltage;
        bmsData.min_cell_voltage = min_voltage;
        
        Serial.print("\"max_cell_voltage_mv\": " + String(max_voltage) + ", ");
        Serial.print("\"max_cell_number\": " + String(max_cell_num) + ", ");
        Serial.print("\"min_cell_voltage_mv\": " + String(min_voltage) + ", ");
        Serial.print("\"min_cell_number\": " + String(min_cell_num) + ", ");
        Serial.print("\"voltage_difference_mv\": " + String(max_voltage - min_voltage));
      }
      break;
      
    case 0x92: // Temperature Range
      {
        uint8_t max_temp_raw = data[4];
        uint8_t max_temp_sensor = data[5];
        uint8_t min_temp_raw = data[6];
        uint8_t min_temp_sensor = data[7];
        
        // Convert from Daly format (offset by 40)
        int8_t max_temp = max_temp_raw - 40;
        int8_t min_temp = min_temp_raw - 40;
        
        bmsData.max_temp = max_temp;
        bmsData.min_temp = min_temp;
        
        Serial.print("\"max_temperature_c\": " + String(max_temp) + ", ");
        Serial.print("\"max_temp_sensor\": " + String(max_temp_sensor) + ", ");
        Serial.print("\"min_temperature_c\": " + String(min_temp) + ", ");
        Serial.print("\"min_temp_sensor\": " + String(min_temp_sensor) + ", ");
        Serial.print("\"temperature_difference_c\": " + String(max_temp - min_temp));
      }
      break;
      
    case 0x93: // Charge/Discharge MOS Status
      {
        uint8_t charge_mos = data[4];
        uint8_t discharge_mos = data[5];
        uint8_t bms_cycles = data[6];
        uint32_t capacity_ah = ((uint32_t)data[7] << 24) | ((uint32_t)data[8] << 16) | ((uint32_t)data[9] << 8) | data[10];
        
        bmsData.protection_status = (charge_mos == 1) && (discharge_mos == 1);
        bmsData.full_capacity = capacity_ah * 0.001; // Convert mAh to Ah
        
        Serial.print("\"charge_mos_enabled\": " + String(charge_mos == 1 ? "true" : "false") + ", ");
        Serial.print("\"discharge_mos_enabled\": " + String(discharge_mos == 1 ? "true" : "false") + ", ");
        Serial.print("\"bms_cycles\": " + String(bms_cycles) + ", ");
        Serial.print("\"capacity_ah\": " + String(bmsData.full_capacity, 3));
      }
      break;
      
    case 0x94: // Status Information
      {
        uint8_t cell_count = data[4];
        uint8_t temp_sensor_count = data[5];
        uint8_t charger_status = data[6];
        uint8_t load_status = data[7];
        uint8_t dio_state = data[8];
        uint16_t cycle_count = (data[9] << 8) | data[10];
        
        bmsData.cycles = cycle_count;
        
        Serial.print("\"cell_count\": " + String(cell_count) + ", ");
        Serial.print("\"temp_sensor_count\": " + String(temp_sensor_count) + ", ");
        Serial.print("\"charger_status\": " + String(charger_status == 1 ? "true" : "false") + ", ");
        Serial.print("\"load_status\": " + String(load_status == 1 ? "true" : "false") + ", ");
        Serial.print("\"cycles\": " + String(cycle_count) + ", ");
        Serial.print("\"dio_state\": \"0x" + String(dio_state, HEX) + "\"");
      }
      break;
      
    default:
      Serial.print("\"error\": \"Unknown command 0x" + String(command, HEX) + "\"");
      break;
  }
  
  // Calculate remaining capacity if we have SOC and full capacity
  if (bmsData.soc > 0 && bmsData.full_capacity > 0) {
    bmsData.remaining_capacity = (bmsData.soc / 100.0) * bmsData.full_capacity;
  }
  
  Serial.print("}");
  delete[] data;
}

uint8_t calculateChecksum(uint8_t* data, int length) {
  uint8_t sum = 0;
  for (int i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum;
}
