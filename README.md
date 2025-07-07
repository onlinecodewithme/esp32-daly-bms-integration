# ESP32 Daly Smart BMS Bluetooth Reader

This project enables an ESP32 to connect to a Daly Smart BMS via Bluetooth Classic and read battery data in real-time.

## Hardware Requirements

- ESP-WROOM-32 development board
- Daly Smart BMS with Bluetooth capability
- USB cable for programming the ESP32

## BMS Configuration

- **BMS MAC Address**: `41:18:12:01:18:9F`
- **Communication Protocol**: Daly BMS Protocol over Bluetooth Classic
- **Data Update Interval**: 5 seconds

## Features

- Automatic Bluetooth connection to Daly BMS
- Real-time battery data reading including:
  - Total voltage (V)
  - Current (A) 
  - State of Charge (SOC %)
  - Maximum and minimum cell voltages (mV)
  - Maximum and minimum temperatures (°C)
  - Protection status
- Automatic reconnection on connection loss
- Serial monitor output for debugging

## Setup Instructions

### 1. Arduino IDE Setup

1. Install Arduino IDE (version 1.8.x or 2.x)
2. Add ESP32 board support:
   - Go to File → Preferences
   - Add this URL to "Additional Board Manager URLs":
     ```
     https://dl.espressif.com/dl/package_esp32_index.json
     ```
   - Go to Tools → Board → Boards Manager
   - Search for "ESP32" and install "ESP32 by Espressif Systems"

### 2. Board Configuration

1. Select your ESP32 board:
   - Tools → Board → ESP32 Arduino → "ESP32 Dev Module"
2. Configure settings:
   - Upload Speed: 921600
   - CPU Frequency: 240MHz (WiFi/BT)
   - Flash Frequency: 80MHz
   - Flash Mode: QIO
   - Flash Size: 4MB (32Mb)
   - Partition Scheme: Default 4MB with spiffs
   - Core Debug Level: None
   - PSRAM: Disabled

### 3. Upload the Code

1. Connect your ESP32 to the computer via USB
2. Select the correct COM port: Tools → Port → [Your ESP32 Port]
3. Open `esp32_daly_bms.ino` in Arduino IDE
4. Click Upload button or press Ctrl+U

### 4. Monitor Output

1. Open Serial Monitor: Tools → Serial Monitor
2. Set baud rate to 115200
3. You should see connection attempts and battery data

## Code Structure

### Main Components

- **BluetoothSerial**: Handles Bluetooth Classic communication
- **DalyCommand**: Structure for BMS command packets
- **BMSData**: Structure to store battery information
- **Connection Management**: Automatic connection and reconnection logic
- **Data Reading**: Multiple command types for different data sets

### Key Functions

- `connectToBMS()`: Establishes Bluetooth connection
- `readBMSData()`: Reads all available battery data
- `sendCommand()`: Sends formatted commands to BMS
- `readResponse()`: Receives and validates BMS responses
- `displayBMSData()`: Outputs formatted data to serial monitor

## Daly BMS Protocol

The program implements the Daly BMS communication protocol with these command types:

| Command | ID   | Description |
|---------|------|-------------|
| 0x90    | Voltage/Current/SOC | Total voltage, current, state of charge |
| 0x91    | Cell Voltages | Maximum and minimum cell voltages |
| 0x92    | Temperatures | Maximum and minimum temperatures |
| 0x93    | Balance Status | Cell balancing information |
| 0x94    | Protection Status | Protection and alarm status |

### Command Structure

```
[Start][Host][BMS][Length][Command][Data(8 bytes)][Checksum]
 0xA5   0x80  0x01  0x08    0x9X    [00 00 ... 00]  [Sum]
```

## Troubleshooting

### Connection Issues

1. **BMS not found**: 
   - Verify BMS MAC address is correct
   - Ensure BMS Bluetooth is enabled
   - Check if BMS is already connected to another device

2. **Connection drops frequently**:
   - Move ESP32 closer to BMS
   - Check power supply stability
   - Verify BMS battery level

3. **No data received**:
   - Check serial monitor for error messages
   - Verify BMS protocol compatibility
   - Try power cycling both devices

### Serial Monitor Output

Expected output when working correctly:
```
ESP32 Daly BMS Bluetooth Reader Starting...
Bluetooth initialized. Attempting to connect to Daly BMS...
Connecting to BMS: 41:18:12:01:18:9F
Successfully connected to Daly BMS!
Reading BMS data...
=== BMS Data ===
Voltage: 12.45 V
Current: -2.30 A
SOC: 85.2 %
Max Cell Voltage: 3125 mV
Min Cell Voltage: 3118 mV
Max Temperature: 25 °C
Min Temperature: 23 °C
Protection Status: NORMAL
================
```

## Customization

### Changing Read Interval

Modify the `READ_INTERVAL` constant in the code:
```cpp
const unsigned long READ_INTERVAL = 10000; // 10 seconds
```

### Adding More Data Types

The Daly BMS supports additional commands. You can extend the code by:
1. Adding new command IDs
2. Creating corresponding read functions
3. Updating the BMSData structure
4. Adding display formatting

### BMS MAC Address

If your BMS has a different MAC address, update this line:
```cpp
const String BMS_MAC = "XX:XX:XX:XX:XX:XX";
```

## License

This project is open source. Feel free to modify and distribute.

## Support

For issues or questions:
1. Check the troubleshooting section
2. Verify hardware connections
3. Review serial monitor output for error messages
