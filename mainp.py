from flask import Flask, render_template, request, jsonify, send_from_directory
import sqlite3
import mysql.connector
import os
import base64
from datetime import datetime
import threading
import time
import paho.mqtt.client as mqtt

app = Flask(__name__)

# MySQL database configuration
db_host = "127.0.0.1"
db_user = "root"
db_password = ""
db_name = "tugasakhir"

# MQTT configuration
mqtt_broker = "broker.emqx.io"
mqtt_topic = "bmkg/kirimcmr"

# Directory to save uploaded images
UPLOAD_FOLDER = 'static/uploads'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# SQLite database initialization
def init_sqlite_db():
    try:
        conn = sqlite3.connect('data.db')
        c = conn.cursor()
        c.execute('''
            CREATE TABLE IF NOT EXISTS images (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                image_path TEXT,
                timestamp TEXT
            )
        ''')
        conn.commit()
    except sqlite3.Error as e:
        print(f"SQLite error: {e}")
    finally:
        conn.close()

init_sqlite_db()

# SQL query to create the genset_data table in MySQL
create_table_query = """
CREATE TABLE IF NOT EXISTS genset_data (
    id INT AUTO_INCREMENT PRIMARY KEY,
    image_path VARCHAR(255) NOT NULL,
    timestamp INT NOT NULL
);
"""

def create_table():
    try:
        conn = mysql.connector.connect(
            host=db_host,
            user=db_user,
            password=db_password,
            database=db_name
        )
        cursor = conn.cursor()
        cursor.execute(create_table_query)
        conn.commit()
        print("Table 'genset_data' created successfully.")
    except mysql.connector.Error as err:
        print(f"Error: {err}")
    finally:
        cursor.close()
        conn.close()

def get_data_from_db():
    try:
        conn = mysql.connector.connect(
            host=db_host,
            user=db_user,
            password=db_password,
            database=db_name
        )
        cursor = conn.cursor()
        query = "SELECT image_path, timestamp FROM genset_data ORDER BY timestamp DESC LIMIT 1"
        cursor.execute(query)
        data = cursor.fetchone()  # Fetch only one row
    except mysql.connector.Error as err:
        print(f"Error: {err}")
        data = None  # Return None if there's an error
    finally:
        if 'cursor' in locals() and cursor is not None:
            cursor.close()
        if 'conn' in locals() and conn is not None:
            conn.close()
    return data

def get_sensor_data_from_db():
    try:
        conn = mysql.connector.connect(
            host=db_host,
            user=db_user,
            password=db_password,
            database=db_name
        )
        cursor = conn.cursor()
        query = "SELECT shunt_voltage, bus_voltage, current, power, voltage, frequency FROM tegangan ORDER BY timestamp DESC LIMIT 1"
        cursor.execute(query)
        data = cursor.fetchall()
    except mysql.connector.Error as err:
        print(f"Error: {err}")
        data = []
    finally:
        cursor.close()
        conn.close()
    return data

@app.route('/')
def index():
    image_data = get_data_from_db()
    sensor_data = get_sensor_data_from_db()
    print(sensor_data)
    return render_template('index.html', image_data=image_data, sensor_data=sensor_data)

@app.route('/publish_mqtt', methods=['POST'])
def publish_mqtt():
    data = request.get_json()
    if not data or 'message' not in data:
        return jsonify(error="Invalid data"), 400
    message = data['message']
    mqtt_client.publish(mqtt_topic, message)
    return jsonify(message="Message published to MQTT"), 200

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    client.subscribe(mqtt_topic)

def parse_sensor_data(payload):
    # Contoh sederhana untuk mendapatkan data sensor dari payload
    # Anda perlu menyesuaikan ini dengan format data sebenarnya yang Anda terima
    try:
        data = payload.decode('utf-8').split(',')  # Misalnya, jika data sensor adalah string terenkripsi
        shunt_voltage = float(data[0])
        bus_voltage = float(data[1])
        current = float(data[2])
        power = float(data[3])
        voltage = float(data[4])
        frequency = float(data[5])
        return (shunt_voltage, bus_voltage, current, power, voltage, frequency)
    except Exception as e:
        print(f"Error parsing sensor data: {e}")
        return (5.0, 3.0, 1.8, 5.4, 220.0, 0.61)  # Mengembalikan nilai default jika ada kesalahan

def on_message(client, userdata, msg):
    try:
        print(f"Received message from topic {msg.topic}")
        # Decode the base64 image
        image_data = msg.payload.decode('utf-8')
        header, encoded = image_data.split(",", 1)
        image = base64.b64decode(encoded)
        
        timestamp = int(time.time())
        file_path = os.path.join(UPLOAD_FOLDER, f"image_{timestamp}.jpg")
        
        with open(file_path, "wb") as f:
            f.write(image)
        
        print(f"Image saved to: {file_path}")
        
        # Insert image path and timestamp into 'genset_data' table
        conn = mysql.connector.connect(
            host=db_host,
            user=db_user,
            password=db_password,
            database=db_name
        )
        cursor = conn.cursor()
        cursor.execute("INSERT INTO genset_data (image_path, timestamp) VALUES (%s, %s)", (file_path, timestamp))
        conn.commit()
        
        # Parse sensor data from MQTT payload
        sensor_data = parse_sensor_data(msg.payload)
        
    except Exception as e:
        print(f"Error in on_message: {e}")
    finally:
        cursor.close()
        conn.close()

        
def run_mqtt_client():
    global mqtt_client
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect(mqtt_broker, 1883, 60)
    mqtt_client.loop_forever()

if __name__ == '__main__':
    create_table()
    mqtt_thread = threading.Thread(target=run_mqtt_client)
    mqtt_thread.start()
    app.run(debug=True)
