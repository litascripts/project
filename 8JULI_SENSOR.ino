#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// MQTT broker information
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* mqtt_client_id = "esp8266_client";

// MQTT topic
const char* mqtt_topic = "/data";

WiFiClient espClient;
PubSubClient client(espClient);

// Configuration for the INA219 sensor
Adafruit_INA219 ina219;

// Pin for the ZMPT101B sensor
const int pinZMPT = A0;

// Variables for frequency measurement
unsigned long previousMillis = 0;
unsigned long interval = 1000; // Interval in milliseconds
unsigned long pulseCount = 0;
unsigned long lastPulseCount = 0;
float frequency = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Measuring INA219 and ZMPT101B...");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read voltage, current, and power from the INA219 sensor
  float shuntvoltage = ina219.getShuntVoltage_mV();
  float busvoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();
  float power_mW = ina219.getPower_mW();

  // Read voltage from the ZMPT101B sensor
  int sensorValue = analogRead(pinZMPT);
  float voltage = sensorValue * (3.3 / 1023.0); // Adjust voltage calculation for 3.3V reference

  // Measure frequency
  unsigned long currentMillis = millis();
  if (analogRead(pinZMPT) > 512) { // Detect zero-crossing
    pulseCount++;
  }
  if (currentMillis - previousMillis >= interval) {
    frequency = (pulseCount - lastPulseCount) * 1000.0 / (currentMillis - previousMillis);
    lastPulseCount = pulseCount;
    previousMillis = currentMillis;
  }

  // Create JSON payload
  StaticJsonDocument<200> jsonDocument;
  jsonDocument["shunt_voltage"] = shuntvoltage;
  jsonDocument["bus_voltage"] = busvoltage;
  jsonDocument["current"] = current_mA;
  jsonDocument["power"] = power_mW;
  jsonDocument["voltage"] = voltage;
  jsonDocument["frequency"] = frequency;

  // Convert JSON to string
  char buffer[512];
  serializeJson(jsonDocument, buffer);

  // Publish JSON payload to MQTT topic
  client.publish(mqtt_topic, buffer);

  // Print data to Serial Monitor
  Serial.println(buffer);

  delay(1000); // Read interval
}
