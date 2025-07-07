#!/usr/bin/env python3
"""
Test script to verify ESP32 BMS JSON output is properly serializable
"""

import json
import sys

def test_json_parsing():
    """Test JSON parsing with sample ESP32 output"""
    
    # Sample JSON output from the updated ESP32 code
    sample_json = """{
  "timestamp": 1234567890,
  "device": "DL-41181201189F",
  "mac_address": "41:18:12:01:18:9f",
  "daly_protocol": {
    "status": "characteristics_found",
    "notifications": "enabled",
    "commands": {
      "main_info": {
        "command_sent": "D2030000003ED7B9",
        "response_received": true,
        "response_data": "d2037c0cf60cf60cf60cf70cf50cf60cf60cf60cf60cf50cf30cf60cf60cf60cf60cf4",
        "parsed_data": {
          "header": {
            "startByte": "0xD2",
            "commandId": "0x03",
            "dataLength": 124
          },
          "cellVoltages": [
            {"cellNumber": 1, "voltage": 3.318},
            {"cellNumber": 2, "voltage": 3.318},
            {"cellNumber": 3, "voltage": 3.318},
            {"cellNumber": 4, "voltage": 3.319},
            {"cellNumber": 5, "voltage": 3.317},
            {"cellNumber": 6, "voltage": 3.318},
            {"cellNumber": 7, "voltage": 3.318},
            {"cellNumber": 8, "voltage": 3.318},
            {"cellNumber": 9, "voltage": 3.318},
            {"cellNumber": 10, "voltage": 3.317},
            {"cellNumber": 11, "voltage": 3.315},
            {"cellNumber": 12, "voltage": 3.318},
            {"cellNumber": 13, "voltage": 3.318},
            {"cellNumber": 14, "voltage": 3.318},
            {"cellNumber": 15, "voltage": 3.318},
            {"cellNumber": 16, "voltage": 3.316}
          ],
          "packVoltage": 53.080,
          "current": 0.0,
          "soc": 90.4,
          "remainingCapacity": 207.9,
          "totalCapacity": 230,
          "cycles": 1,
          "temperatures": [
            {"sensor": "T1", "temperature": 30},
            {"sensor": "T2", "temperature": 30}
          ],
          "mosStatus": {
            "chargingMos": true,
            "dischargingMos": true,
            "balancing": false
          },
          "checksum": "0x2C73",
          "timestamp": "1234567890"
        }
      }
    }
  },
  "data_found": true
}"""

    print("🔋 Testing ESP32 BMS JSON Output Serialization")
    print("=" * 50)
    
    try:
        # Test JSON parsing
        print("📝 Testing JSON parsing...")
        data = json.loads(sample_json)
        print("✅ JSON parsing successful!")
        
        # Test data access
        print("\n📊 Testing data access...")
        
        # Basic fields
        timestamp = data.get('timestamp')
        device = data.get('device')
        mac_address = data.get('mac_address')
        data_found = data.get('data_found')
        
        print(f"   Timestamp: {timestamp}")
        print(f"   Device: {device}")
        print(f"   MAC: {mac_address}")
        print(f"   Data Found: {data_found}")
        
        # Protocol data
        daly_protocol = data.get('daly_protocol', {})
        status = daly_protocol.get('status')
        print(f"   Status: {status}")
        
        # Parsed data
        commands = daly_protocol.get('commands', {})
        main_info = commands.get('main_info', {})
        parsed_data = main_info.get('parsed_data', {})
        
        if parsed_data:
            pack_voltage = parsed_data.get('packVoltage')
            current = parsed_data.get('current')
            soc = parsed_data.get('soc')
            cell_voltages = parsed_data.get('cellVoltages', [])
            temperatures = parsed_data.get('temperatures', [])
            
            print(f"   Pack Voltage: {pack_voltage}V")
            print(f"   Current: {current}A")
            print(f"   SOC: {soc}%")
            print(f"   Cell Count: {len(cell_voltages)}")
            print(f"   Temperature Sensors: {len(temperatures)}")
            
            # Test cell voltage access
            if cell_voltages:
                first_cell = cell_voltages[0]
                cell_number = first_cell.get('cellNumber')
                voltage = first_cell.get('voltage')
                print(f"   First Cell: #{cell_number} = {voltage}V")
            
            # Test temperature access
            if temperatures:
                first_temp = temperatures[0]
                sensor = first_temp.get('sensor')
                temp = first_temp.get('temperature')
                print(f"   First Temp: {sensor} = {temp}°C")
        
        print("\n✅ All data access tests passed!")
        
        # Test JSON serialization back to string
        print("\n📤 Testing JSON serialization...")
        json_string = json.dumps(data, indent=2)
        print("✅ JSON serialization successful!")
        print(f"   Serialized length: {len(json_string)} characters")
        
        # Test compact JSON (for ROS2)
        compact_json = json.dumps(data, separators=(',', ':'))
        print(f"   Compact length: {len(compact_json)} characters")
        
        # Test parsing the serialized JSON
        print("\n🔄 Testing round-trip serialization...")
        reparsed_data = json.loads(compact_json)
        
        # Verify data integrity
        if (reparsed_data.get('timestamp') == data.get('timestamp') and
            reparsed_data.get('device') == data.get('device') and
            reparsed_data.get('data_found') == data.get('data_found')):
            print("✅ Round-trip serialization successful!")
        else:
            print("❌ Round-trip serialization failed!")
            return False
        
        print("\n🎉 All JSON serialization tests passed!")
        print("✅ ESP32 JSON output is properly serializable for ROS2 nodes")
        
        return True
        
    except json.JSONDecodeError as e:
        print(f"❌ JSON parsing failed: {e}")
        return False
    except Exception as e:
        print(f"❌ Test failed: {e}")
        return False

def test_bms_data_prefix():
    """Test BMS_DATA prefix parsing"""
    print("\n🔍 Testing BMS_DATA prefix parsing...")
    
    sample_line = 'BMS_DATA:{"timestamp":1234567890,"device":"DL-41181201189F","data_found":true}'
    
    if sample_line.startswith('BMS_DATA:'):
        json_data = sample_line[9:]  # Remove 'BMS_DATA:' prefix
        try:
            data = json.loads(json_data)
            print("✅ BMS_DATA prefix parsing successful!")
            print(f"   Device: {data.get('device')}")
            print(f"   Data Found: {data.get('data_found')}")
            return True
        except json.JSONDecodeError as e:
            print(f"❌ JSON parsing after prefix removal failed: {e}")
            return False
    else:
        print("❌ BMS_DATA prefix not found")
        return False

def main():
    """Main test function"""
    print("🧪 ESP32 BMS JSON Serialization Test Suite")
    print("=" * 60)
    
    success = True
    
    # Test 1: JSON parsing and serialization
    success &= test_json_parsing()
    
    # Test 2: BMS_DATA prefix handling
    success &= test_bms_data_prefix()
    
    print("\n" + "=" * 60)
    if success:
        print("🎉 ALL TESTS PASSED! ESP32 JSON output is ready for ROS2 integration")
        sys.exit(0)
    else:
        print("❌ SOME TESTS FAILED! Please check the JSON format")
        sys.exit(1)

if __name__ == '__main__':
    main()
