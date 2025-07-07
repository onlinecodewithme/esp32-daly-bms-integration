/*
 * Utility functions for ESP32 Daly BMS Reader
 * Contains helper functions for data formatting, logging, and JSON output
 */

#ifndef UTILS_H
#define UTILS_H

#include "Arduino.h"
#include "config.h"

// JSON output functions
String createJSONOutput(const BMSData& data) {
  String json = "{";
  json += "\"timestamp\":" + String(millis()) + ",";
  json += "\"voltage\":" + String(data.voltage, 2) + ",";
  json += "\"current\":" + String(data.current, 2) + ",";
  json += "\"soc\":" + String(data.soc, 1) + ",";
  json += "\"max_cell_voltage\":" + String(data.max_cell_voltage) + ",";
  json += "\"min_cell_voltage\":" + String(data.min_cell_voltage) + ",";
  json += "\"max_temperature\":" + String(data.max_temp) + ",";
  json += "\"min_temperature\":" + String(data.min_temp) + ",";
  json += "\"protection_status\":" + String(data.protection_status ? "true" : "false") + ",";
  json += "\"remaining_capacity\":" + String(data.remaining_capacity, 2) + ",";
  json += "\"full_capacity\":" + String(data.full_capacity, 2);
  json += "}";
  return json;
}

// Debug function to print raw hex data
void printHexData(const uint8_t* data, int length, const String& label) {
  if (DEBUG_RAW_DATA) {
    Serial.print(label + ": ");
    for (int i = 0; i < length; i++) {
      if (data[i] < 0x10) Serial.print("0");
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}

// Function to validate BMS response
bool validateResponse(const uint8_t* response, int length, uint8_t expectedCommand) {
  if (length < 4) return false;
  
  // Check start byte
  if (response[0] != DALY_START_BYTE) {
    if (DEBUG_ENABLED) {
      Serial.println("Invalid start byte: 0x" + String(response[0], HEX));
    }
    return false;
  }
  
  // Check command echo
  if (response[4] != expectedCommand) {
    if (DEBUG_ENABLED) {
      Serial.println("Command mismatch. Expected: 0x" + String(expectedCommand, HEX) + 
                    ", Got: 0x" + String(response[4], HEX));
    }
    return false;
  }
  
  return true;
}

// Function to calculate and verify checksum
bool verifyChecksum(const uint8_t* data, int length) {
  if (length < 2) return false;
  
  uint8_t calculatedSum = 0;
  for (int i = 0; i < length - 1; i++) {
    calculatedSum += data[i];
  }
  
  uint8_t receivedChecksum = data[length - 1];
  
  if (calculatedSum != receivedChecksum) {
    if (DEBUG_ENABLED) {
      Serial.println("Checksum mismatch. Calculated: 0x" + String(calculatedSum, HEX) + 
                    ", Received: 0x" + String(receivedChecksum, HEX));
    }
    return false;
  }
  
  return true;
}

// Function to format uptime
String formatUptime(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  
  String uptime = "";
  if (days > 0) uptime += String(days) + "d ";
  if (hours > 0) uptime += String(hours) + "h ";
  if (minutes > 0) uptime += String(minutes) + "m ";
  uptime += String(seconds) + "s";
  
  return uptime;
}

// Function to get battery status description
String getBatteryStatus(const BMSData& data) {
  if (data.protection_status) {
    return "PROTECTION ACTIVE";
  } else if (data.current > 0.1) {
    return "CHARGING";
  } else if (data.current < -0.1) {
    return "DISCHARGING";
  } else {
    return "IDLE";
  }
}

// Function to get SOC status
String getSOCStatus(float soc) {
  if (soc >= 80) return "HIGH";
  else if (soc >= 50) return "MEDIUM";
  else if (soc >= 20) return "LOW";
  else return "CRITICAL";
}

// Function to get temperature status
String getTemperatureStatus(int temp) {
  if (temp >= 45) return "HOT";
  else if (temp >= 35) return "WARM";
  else if (temp >= 10) return "NORMAL";
  else if (temp >= 0) return "COLD";
  else return "FREEZING";
}

// Function to create detailed status report
void printDetailedStatus(const BMSData& data) {
  Serial.println("=== DETAILED BMS STATUS ===");
  Serial.println("System Uptime: " + formatUptime(millis()));
  Serial.println("Battery Status: " + getBatteryStatus(data));
  Serial.println("SOC Level: " + getSOCStatus(data.soc));
  Serial.println("Temperature: " + getTemperatureStatus(data.max_temp));
  
  // Power calculation
  float power = data.voltage * data.current;
  Serial.println("Power: " + String(power, 2) + " W");
  
  // Cell voltage difference
  if (data.max_cell_voltage > 0 && data.min_cell_voltage > 0) {
    int voltage_diff = data.max_cell_voltage - data.min_cell_voltage;
    Serial.println("Cell Voltage Difference: " + String(voltage_diff) + " mV");
    
    if (voltage_diff > 100) {
      Serial.println("WARNING: High cell voltage imbalance!");
    }
  }
  
  // Temperature difference
  if (data.max_temp > data.min_temp) {
    int temp_diff = data.max_temp - data.min_temp;
    Serial.println("Temperature Difference: " + String(temp_diff) + " °C");
    
    if (temp_diff > 10) {
      Serial.println("WARNING: High temperature difference!");
    }
  }
  
  Serial.println("===========================");
  Serial.println();
}

// Function to log data to serial in CSV format (for data logging)
void logCSVData(const BMSData& data) {
  static bool headerPrinted = false;
  
  if (!headerPrinted) {
    Serial.println("Timestamp,Voltage,Current,SOC,MaxCellV,MinCellV,MaxTemp,MinTemp,Protection,Power");
    headerPrinted = true;
  }
  
  float power = data.voltage * data.current;
  
  Serial.print(millis());
  Serial.print(",");
  Serial.print(data.voltage, 2);
  Serial.print(",");
  Serial.print(data.current, 2);
  Serial.print(",");
  Serial.print(data.soc, 1);
  Serial.print(",");
  Serial.print(data.max_cell_voltage);
  Serial.print(",");
  Serial.print(data.min_cell_voltage);
  Serial.print(",");
  Serial.print(data.max_temp);
  Serial.print(",");
  Serial.print(data.min_temp);
  Serial.print(",");
  Serial.print(data.protection_status ? 1 : 0);
  Serial.print(",");
  Serial.println(power, 2);
}

#endif // UTILS_H
