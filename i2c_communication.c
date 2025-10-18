import smbus2
import struct
import time

# Define the Arduino I2C address
ARDUINO_ADDRESS = 0x04

# Initialize I2C bus
bus = smbus2.SMBus(1)  # Use the appropriate bus number

def receive_data():
    try:
        # Request data from Arduino
        bus.write_byte(ARDUINO_ADDRESS, 0)

        # Read temperature and humidity data as a single block (8 bytes)
        data = bus.read_i2c_block_data(ARDUINO_ADDRESS, 0, 8)
        
        # Unpack the data into temperature and humidity values
        temperature, humidity = struct.unpack('ff', bytearray(data))

        return temperature, humidity
    except Exception as e:
        print(f"Error receiving data from Arduino: {e}")
        return None, None

def main():
    while True:
        # Receive temperature and humidity data from Arduino
        temperature, humidity = receive_data()
        print("Temperature and Humidity data")
        
        # Print received data
        if temperature is not None and humidity is not None:
            print(f"Temperature: {temperature:.2f} \u00b0C,  Humidity: {humidity:.2f}%\n")
		
        time.sleep(3)
        

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        pass
