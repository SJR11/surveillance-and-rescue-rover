import smbus2
import struct
import time
import threading
import socketserver
from http import server
from http.server import BaseHTTPRequestHandler, HTTPServer
import io
from threading import Condition
from picamera2 import Picamera2
from picamera2.encoders import JpegEncoder
from picamera2.outputs import FileOutput
import json


	# Define the Arduino I2C address
ARDUINO_ADDRESS = 0x04

	# Initialize I2C bus
bus = smbus2.SMBus(1)  # pi4 uses port 1 for i2c communication


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

        # Print received data
        if temperature is not None and humidity is not None:
            print(f"Temperature: {temperature:.2f} \u00b0C, Humidity: {humidity:.2f}%")

        time.sleep(2)


# HTML page for the MJPEG streaming
PAGE = """\
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Surveillance Rover</title>
    <style>
		.top1 h1 {
				text-align: center;
		}
		.top1 h2 {
            text-align: left; 
            margin-left: auto; 
            margin-right: auto; 
		}

        .container {
            display: flex;
            flex-wrap: wrap;
        }
        .video-stream {
            flex: 1;
            margin-right: 30px; 
        }
        .box {
            border: 1px solid #ccc;
            padding: 10px;
            margin-top: 20px;
            margin-left: 30px;
            width: 350px;
            text-align: center;
        }
        table {
            border-collapse: collapse;
            width: 100%;
        }
        th, td {
            border: 1px solid #dddddd;
            text-align: left;
            padding: 8px;
        }
    </style>
</head>
<body>
    <div class="top1">
        <h1><b>Welcome To The BotMinds Surveillance Rover</b></h1>
        <h2><b>Video Streaming</b></h2>
    </div>

    <!-- Wrap video streaming and box in a container -->
    <div class="container">
        <div class="video-stream">
            <img class="liveserver" src="stream.mjpg" width="1000" height="500" alt="live Video">
        </div>

        <!-- New box for temperature and humidity data -->
        <div class="box">
            <h3><b>Temperature & Humidity Readings</b></h3>
            <p>Temperature: <span id="temperature">--</span> &deg;C</p>
            <p>Humidity: <span id="humidity">--</span> %</p>
        </div>
    </div>

     <!-- JavaScript to update temperature and humidity data -->
    <script>
        // Function to update temperature and humidity data from the server
        function updateTemperatureAndHumidity() {
            // Fetch data from the server
            fetch("/get_data")
                .then(response => response.json()) // Parse JSON response
                .then(data => {
                    // Update the HTML elements with the new data
                    document.getElementById("temperature").textContent = data.temperature.toFixed(2);
                    document.getElementById("humidity").textContent = data.humidity.toFixed(2);
                })
                .catch(error => console.error('Error fetching data:', error));
        }

        // Function to update temperature and humidity data initially
        function updateInitialData() {
            // Fetch initial data from the server
            fetch("/initial_data")
                .then(response => response.json()) // Parse JSON response
                .then(data => {
                    // Update the HTML elements with the initial data
                    document.getElementById("temperature").textContent = data.temperature.toFixed(2);
                    document.getElementById("humidity").textContent = data.humidity.toFixed(2);
                })
                .catch(error => console.error('Error fetching initial data:', error));
        }

        // Call the function to update initial data
        updateInitialData();
        // Call the function to update data at intervals
        setInterval(updateTemperatureAndHumidity, 3000); // Update every 3 seconds
    </script>
</body>
</html>
"""
# Class to handle streaming output
class StreamingOutput(io.BufferedIOBase):
    def __init__(self):
        self.frame = None
        self.condition = Condition()

    def write(self, buf):
        with self.condition:
            self.frame = buf
            self.condition.notify_all()


# Class to handle HTTP requests
class StreamingHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            # Redirect root path to index.html
            self.send_response(301)
            self.send_header('Location', '/index.html')
            self.end_headers()
        elif self.path == '/index.html':
            # Serve the HTML page
            content = PAGE.encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.send_header('Content-Length', len(content))
            self.end_headers()
            self.wfile.write(content)
        elif self.path == '/stream.mjpg':
            # Set up MJPEG streaming
            self.send_response(200)
            self.send_header('Age', 0)
            self.send_header('Cache-Control', 'no-cache, private')
            self.send_header('Pragma', 'no-cache')
            self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=FRAME')
            self.end_headers()
            try:
                while True:
                    with output.condition:
                        output.condition.wait()
                        frame = output.frame
                    self.wfile.write(b'--FRAME\r\n')
                    self.send_header('Content-Type', 'image/jpeg')
                    self.send_header('Content-Length', len(frame))
                    self.end_headers()
                    self.wfile.write(frame)
                    self.wfile.write(b'\r\n')
            except Exception as e:
                logging.warning(
                    'Removed streaming client %s: %s',
                    self.client_address, str(e))
        elif self.path == '/get_data':
            # Return current temperature and humidity data
            temperature, humidity = receive_data()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({"temperature": temperature, "humidity": humidity}).encode('utf-8'))
        elif self.path == '/initial_data':
            # Return initial temperature and humidity data
            initial_temperature, initial_humidity = 0, 0  # Replace with your initial values
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({"temperature": initial_temperature, "humidity": initial_humidity}).encode('utf-8'))
        else:
            # Handle 404 Not Found
            self.send_error(404)
            self.end_headers()


# Class to handle streaming server
class StreamingServer(socketserver.ThreadingMixIn, server.HTTPServer):
    allow_reuse_address = True
    daemon_threads = True


# Create Picamera2 instance and configure it
picam2 = Picamera2()
picam2.configure(picam2.create_video_configuration(main={"size": (640, 480)}))
output = StreamingOutput()
picam2.start_recording(JpegEncoder(), FileOutput(output))

try:
    # Set up and start the streaming server
    address = ('', 8000)
    server = StreamingServer(address, StreamingHandler)
    server.serve_forever()
finally:
    # Stop recording when the script is interrupted
    picam2.stop_recording()
