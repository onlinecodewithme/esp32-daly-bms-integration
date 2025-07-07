# ESP32 Daly BMS Integration - Installation Guide

This guide will walk you through setting up your ESP32 to communicate with your Daly Smart BMS via Bluetooth.

## Quick Start

1. **Hardware Setup**: Connect your ESP-WROOM-32 to your computer via USB
2. **Software Setup**: Install Arduino IDE and ESP32 board support
3. **Upload Code**: Flash the program to your ESP32
4. **Connect**: The ESP32 will automatically connect to your BMS
5. **Monitor**: View battery data in the Serial Monitor

## Detailed Installation Steps

### Step 1: Hardware Requirements

✅ **Required Components:**
- ESP-WROOM-32 development board
- USB cable (typically USB-A to Micro-USB or USB-C)
- Daly Smart BMS with Bluetooth enabled
- Computer with Arduino IDE

✅ **BMS Configuration:**
- MAC Address: `41:18:12:01:18:9F`
- Ensure Bluetooth is enabled on your BMS
- BMS should be powered and operational

### Step 2: Arduino IDE Setup

1. **Download Arduino IDE**
   - Visit: https://www.arduino.cc/en/software
   - Download version 1.8.19 or newer (or Arduino IDE 2.x)
   - Install following the standard procedure for your OS

2. **Add ESP32 Board Support**
   ```
   File → Preferences → Additional Board Manager URLs
   Add: https://dl.espressif.com/dl/package_esp32_index.json
   ```
   
   ```
   Tools → Board → Boards Manager
   Search: "ESP32"
   Install: "ESP32 by Espressif Systems"
   ```

3. **Install Required Libraries**
   - BluetoothSerial library is included with ESP32 board package
   - No additional libraries needed

### Step 3: Board Configuration

Select the correct board and settings:

```
Tools → Board → ESP32 Arduino → "ESP32 Dev Module"
Tools → Upload Speed → 921600
Tools → CPU Frequency → 240MHz (WiFi/BT)
Tools → Flash Frequency → 80MHz
Tools → Flash Mode → QIO
Tools → Flash Size → 4MB (32Mb)
Tools → Partition Scheme → Default 4MB with spiffs
```

### Step 4: Code Upload

1. **Connect ESP32**
   - Connect ESP32 to computer via USB
   - Select correct port: `Tools → Port → [Your ESP32 Port]`
   - Port names vary by OS:
     - Windows: COM3, COM4, etc.
     - macOS: /dev/cu.usbserial-xxx
     - Linux: /dev/ttyUSB0, /dev/ttyACM0

2. **Choose Your Version**
   
   **Basic Version** (`esp32_daly_bms.ino`):
   - Simple, straightforward implementation
   - Basic error handling
   - Standard serial output
   
   **Enhanced Version** (`esp32_daly_bms_enhanced.ino`):
   - Advanced error handling and reconnection logic
   - Multiple output formats (Normal, JSON, CSV)
   - Interactive serial commands
   - Detailed status reporting
   - Modular design with config.h and utils.h

3. **Upload Process**
   - Open your chosen .ino file in Arduino IDE
   - Click Upload button (→) or press Ctrl+U
   - Wait for "Done uploading" message

### Step 5: Testing and Monitoring

1. **Open Serial Monitor**
   ```
   Tools → Serial Monitor
   Set baud rate to: 115200
   ```

2. **Expected Output**
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
   ...
   ```

## File Structure

```
bms_integration/
├── esp32_daly_bms.ino              # Basic version
├── esp32_daly_bms_enhanced.ino     # Enhanced version
├── config.h                        # Configuration settings
├── utils.h                         # Utility functions
├── README.md                       # Project documentation
└── INSTALLATION_GUIDE.md           # This file
```

## Enhanced Version Features

If you choose the enhanced version, you'll have access to these serial commands:

| Command | Description |
|---------|-------------|
| `help` or `h` | Show available commands |
| `status` or `s` | Show system status |
| `detailed` or `d` | Show detailed BMS analysis |
| `json` or `j` | Switch to JSON output format |
| `csv` or `c` | Switch to CSV output format |
| `normal` or `n` | Switch to normal output format |
| `reconnect` or `r` | Force reconnection to BMS |
| `debug` | Show debug information |

## Troubleshooting

### Connection Issues

**Problem**: "Failed to connect to BMS"
- ✅ Verify BMS MAC address is correct
- ✅ Ensure BMS Bluetooth is enabled
- ✅ Check if BMS is already connected to another device
- ✅ Move ESP32 closer to BMS (within 10 meters)
- ✅ Power cycle both ESP32 and BMS

**Problem**: "Invalid MAC address format"
- ✅ Check MAC address format in code: `XX:XX:XX:XX:XX:XX`
- ✅ Ensure all characters are valid hexadecimal

**Problem**: ESP32 not detected by computer
- ✅ Try different USB cable
- ✅ Install CP210x or CH340 drivers if needed
- ✅ Check Device Manager (Windows) for COM ports

### Data Reading Issues

**Problem**: "Failed to read valid response"
- ✅ Check BMS compatibility (Daly BMS protocol)
- ✅ Verify power supply to ESP32 is stable
- ✅ Try reducing read interval in config.h
- ✅ Enable debug mode to see raw data

**Problem**: Inconsistent data readings
- ✅ Check for electromagnetic interference
- ✅ Ensure stable power supply
- ✅ Verify BMS is not in sleep mode

### Serial Monitor Issues

**Problem**: No output in Serial Monitor
- ✅ Check baud rate is set to 115200
- ✅ Verify correct COM port is selected
- ✅ Try closing and reopening Serial Monitor

## Customization

### Changing BMS MAC Address

Edit `config.h`:
```cpp
#define BMS_MAC_ADDRESS "XX:XX:XX:XX:XX:XX"
```

### Adjusting Read Interval

Edit `config.h`:
```cpp
#define READ_INTERVAL 10000  // 10 seconds
```

### Enabling Debug Output

Edit `config.h`:
```cpp
#define DEBUG_ENABLED true
#define DEBUG_RAW_DATA true  // Show hex data
```

## Advanced Usage

### Data Logging

For continuous data logging, use CSV mode:
1. Type `csv` in Serial Monitor
2. Copy output to spreadsheet application
3. Analyze trends over time

### JSON Integration

For integration with other systems:
1. Type `json` in Serial Monitor
2. Parse JSON output in your application
3. Use timestamp field for data synchronization

### Remote Monitoring

Consider adding WiFi capability to send data to cloud services or local servers.

## Support and Maintenance

### Regular Maintenance
- Monitor for connection stability
- Check for firmware updates
- Verify BMS battery levels regularly

### Performance Optimization
- Adjust read intervals based on your needs
- Use appropriate output format for your use case
- Monitor ESP32 memory usage for long-term operation

### Safety Considerations
- Always monitor BMS protection status
- Set up alerts for critical battery conditions
- Ensure proper ventilation for both ESP32 and BMS

## Next Steps

After successful installation:
1. Monitor your battery system regularly
2. Set up data logging for trend analysis
3. Consider integrating with home automation systems
4. Explore additional BMS commands for more detailed data

For technical support or questions, refer to the troubleshooting section or check the project documentation.
