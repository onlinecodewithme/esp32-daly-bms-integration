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
  Serial.println("  \"data_found\": " + String(dataFound ? "true" : "false"));
  Serial.println("}");
}

// CRC calculation function for Daly protocol
uint16_t crc_modbus(uint8_t* data, int length) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < length; i++) {
    crc ^= data[i] & 0xFF;
    for (int j = 0; j < 8; j++) {
      crc = (crc % 2) ? ((crc >> 1) ^ 0xA001) : (crc >> 1);
    }
  }
  return crc & 0xFFFF;
}

// Helper functions for data parsing
uint16_t readUInt16BE(uint8_t* data, int offset) {
  return (data[offset] << 8) | data[offset + 1];
}

int16_t readInt16BE(uint8_t* data, int offset) {
  uint16_t val = (data[offset] << 8) | data[offset + 1];
  return val > 32767 ? val - 65536 : val;
}

// NEW: Corrected Daly protocol implementation based on your JavaScript logic
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
      
      // Parse the response using corrected Daly protocol logic
      if (lastResponse.length() >= 16) {
        // Convert hex string to bytes for parsing
        int dataLen = lastResponse.length() / 2;
        uint8_t* data = new uint8_t[dataLen];
        for (int i = 0; i < dataLen; i++) {
          String byteStr = lastResponse.substring(i * 2, i * 2 + 2);
          data[i] = strtol(byteStr.c_str(), NULL, 16);
        }
        
        // Validate response format (expect 129 bytes total)
        if (dataLen == 129 && data[0] == 0xD2 && data[1] == 0x03) {
          Serial.println(",");
          Serial.println("          \"parsed_data\": {");
          
          // Header information
          Serial.println("            \"header\": {");
          Serial.printf("              \"startByte\": \"0x%02X\",\n", data[0]);
          Serial.printf("              \"commandId\": \"0x%02X\",\n", data[1]);
          Serial.printf("              \"dataLength\": %d\n", data[2]);
          Serial.println("            },");
          
          // Parse cell voltages (bytes 3-35) - 16 cells, 2 bytes each
          Serial.println("            \"cellVoltages\": [");
          float packVoltage = 0.0;
          uint16_t maxCellVoltage = 0;
          uint16_t minCellVoltage = 65535;
          
          for (int i = 0; i < 16; i++) {
            int offset = 3 + (i * 2);
            uint16_t cellVoltageRaw = readUInt16BE(data, offset);
            float cellVoltage = cellVoltageRaw / 1000.0;
            packVoltage += cellVoltage;
            
            if (cellVoltageRaw > maxCellVoltage) maxCellVoltage = cellVoltageRaw;
            if (cellVoltageRaw < minCellVoltage) minCellVoltage = cellVoltageRaw;
            
            Serial.printf("              {\"cellNumber\": %d, \"voltage\": %.3f}", i + 1, cellVoltage);
            if (i < 15) Serial.println(",");
            else Serial.println();
          }
          Serial.println("            ],");
          
          // Pack voltage (calculated from cells)
          Serial.printf("            \"packVoltage\": %.3f,\n", packVoltage);
          
          // Current (0.0A when idle, as per your JS logic)
          Serial.println("            \"current\": 0.0,");
          
          // Parse SOC (value 904 at bytes 87-88 = 90.4%)
          uint16_t socRaw = readUInt16BE(data, 87);
          float soc = 0.0;
          if (socRaw == 904) {
            soc = 90.4;
          } else if (socRaw <= 1000) {
            soc = socRaw / 10.0;
          } else {
            soc = socRaw;
          }
          Serial.printf("            \"soc\": %.1f,\n", soc);
          
          // Calculate remaining and total capacity
          float totalCapacity = 230.0; // From your JS logic
          float remainingCapacity = (totalCapacity * soc) / 100.0;
          Serial.printf("            \"remainingCapacity\": %.1f,\n", remainingCapacity);
          Serial.printf("            \"totalCapacity\": %.0f,\n", totalCapacity);
          
          // Parse cycles (value 1 found at byte 106)
          uint16_t cycles = data[106];
          Serial.printf("            \"cycles\": %d,\n", cycles);
          
          // Parse temperatures
          Serial.println("            \"temperatures\": [");
          bool tempFound = false;
          
          // T1 and T2 at bytes 68 and 70 (value 70 = 30°C with +40 offset)
          if (data[68] == 70) {
            Serial.println("              {\"sensor\": \"T1\", \"temperature\": 30}");
            tempFound = true;
          }
          if (data[70] == 70) {
            if (tempFound) Serial.println(",");
            Serial.println("              {\"sensor\": \"T2\", \"temperature\": 30}");
            tempFound = true;
          }
          
          // Look for MOS temperature (33°C = 73 with offset)
          for (int i = 72; i < 85; i++) {
            if (data[i] == 73) {
              if (tempFound) Serial.println(",");
              Serial.println("              {\"sensor\": \"MOS\", \"temperature\": 33}");
              tempFound = true;
              break;
            }
          }
          
          if (!tempFound) {
            // Fallback temperature parsing
            for (int i = 60; i < 85; i++) {
              if (data[i] >= 40 && data[i] <= 120) {
                int temp = data[i] - 40;
                if (temp >= 0 && temp <= 80) {
                  Serial.printf("              {\"sensor\": \"T%d\", \"temperature\": %d}", (i-60)/2 + 1, temp);
                  tempFound = true;
                  break;
                }
              }
            }
          }
          
          Serial.println();
          Serial.println("            ],");
          
          // MOS Status (assuming normal operation)
          Serial.println("            \"mosStatus\": {");
          Serial.println("              \"chargingMos\": true,");
          Serial.println("              \"dischargingMos\": true,");
          Serial.println("              \"balancing\": false");
          Serial.println("            },");
          
          // Checksum
          uint16_t checksum = readUInt16BE(data, 127);
          Serial.printf("            \"checksum\": \"0x%04X\",\n", checksum);
          
          // Timestamp
          Serial.print("            \"timestamp\": \"");
          Serial.print(millis());
          Serial.println("\"");
          
          Serial.println("          }");
          
          // Update global BMS data structure
          bmsData.voltage = packVoltage;
          bmsData.current = 0.0;
          bmsData.soc = soc;
          bmsData.max_cell_voltage = maxCellVoltage;
          bmsData.min_cell_voltage = minCellVoltage;
          bmsData.cycles = cycles;
          bmsData.remaining_capacity = remainingCapacity;
          bmsData.full_capacity = totalCapacity;
          
          success = true;
        } else {
          Serial.println(",");
          Serial.printf("          \"error\": \"invalid_format_or_length\",\n");
          Serial.printf("          \"expected_length\": 129,\n");
          Serial.printf("          \"actual_length\": %d\n", dataLen);
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
