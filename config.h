/*
 * Configuration file for ESP32 Daly BMS Reader
 * Modify these settings according to your setup
 */

#ifndef CONFIG_H
#define CONFIG_H

// BMS Configuration
#define BMS_MAC_ADDRESS "41:18:12:01:18:9F"
#define BMS_DEVICE_NAME "DalyBMS"
#define ESP32_BT_NAME "ESP32_BMS_Reader"

// Communication Settings
#define SERIAL_BAUD_RATE 115200
#define BT_CONNECTION_TIMEOUT 10000  // 10 seconds
#define READ_INTERVAL 5000           // 5 seconds between readings
#define RESPONSE_TIMEOUT 1000        // 1 second for BMS response

// Daly BMS Protocol Constants
#define DALY_START_BYTE 0xA5
#define DALY_HOST_ADDR 0x80
#define DALY_BMS_ADDR 0x01
#define DALY_DATA_LEN 0x08

// Command IDs
#define CMD_VOUT_IOUT_SOC 0x90       // Voltage, Current, SOC
#define CMD_MIN_MAX_CELL_VOLTAGE 0x91 // Min/Max cell voltages
#define CMD_MIN_MAX_TEMPERATURE 0x92  // Min/Max temperatures
#define CMD_DISCHARGE_CHARGE_MOS 0x93 // MOS status
#define CMD_STATUS_INFO 0x94          // Status information
#define CMD_CELL_VOLTAGES 0x95        // Individual cell voltages
#define CMD_CELL_TEMPERATURE 0x96     // Individual cell temperatures
#define CMD_CELL_BALANCE_STATE 0x97   // Cell balance state
#define CMD_FAILURE_CODES 0x98        // Failure codes

// Data parsing constants
#define VOLTAGE_SCALE 0.1f           // 0.1V per unit
#define CURRENT_SCALE 0.1f           // 0.1A per unit
#define SOC_SCALE 0.1f               // 0.1% per unit
#define CELL_VOLTAGE_SCALE 1.0f      // 1mV per unit
#define TEMPERATURE_OFFSET 40        // Temperature offset (°C)

// Debug settings
#define DEBUG_ENABLED true
#define DEBUG_RAW_DATA false         // Set to true to see raw hex data

// Connection retry settings
#define MAX_RECONNECT_ATTEMPTS 5
#define RECONNECT_DELAY 5000         // 5 seconds between reconnect attempts

#endif // CONFIG_H
