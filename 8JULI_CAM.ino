#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"
#include "Base64.h"

// WiFi credentials and MQTT server details
const char* ssid = "xxx";
const char* password = "xxx";
const char* mqtt_server = "broker.emqx.io";
const unsigned int mqtt_port = 1883;
#define MQTT_USER               ""
#define MQTT_PASSWORD           ""
#define MQTT_PUBLISH_TOPIC      ""
#define MQTT_SUBSCRIBE_TOPIC    ""

WiFiClient espClient;
PubSubClient client(espClient);

// Camera pins configuration
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

String command="", cmd="", P1="", P2="", P3="", P4="", P5="", P6="", P7="", P8="", P9="";

// Timing for camera capture and data sending
const long interval = 7000;  // 7 seconds interval
unsigned long previousMillis = 0;

void executeCommand() {
  String feedback = "";
  Serial.print(cmd);
  if (cmd == "PING") {
    feedback = "Selamat Anda Terhubung!";
  } else if (cmd == "ip") {
    feedback = WiFi.localIP().toString();
  } else if (cmd == "mac") {
    feedback = WiFi.macAddress();
  } else if (cmd == "restart") {
    ESP.restart();
  } else if (cmd == "resetwifi") {
    WiFi.begin(P1.c_str(), P2.c_str());
    long int StartTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if ((StartTime + 5000) < millis()) break;
    } 
    feedback = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "failed";
  } else if (cmd == "ambilfoto") {
    feedback = sendImage();
  } else {
    feedback = "Command is not defined";
  } 
  if (!feedback.isEmpty()) sendText(feedback);
}

void setup() {
  Serial.begin(115200);
  randomSeed(micros());
  initCamera();
  initWiFi();
  client.setBufferSize(4096);  // Set buffer size before connecting to the server
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sendImage();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  command = ""; cmd = ""; P1 = ""; P2 = ""; P3 = ""; P4 = ""; P5 = ""; P6 = ""; P7 = ""; P8 = ""; P9 = "";
  String input((char*)payload, length);
  deserializeCommand(input);
  if (!cmd.isEmpty()) executeCommand();
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      client.subscribe(MQTT_SUBSCRIBE_TOPIC);
    } else {
      Serial.println("Failed to connect to MQTT, retry in 5 seconds");
      delay(5000);
    }
  }
}

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Adjust the frame size and JPEG quality for smaller images
  config.frame_size = FRAMESIZE_QVGA;  // Smaller resolution for smaller image size
  config.jpeg_quality = 30;  // Higher compression for smaller image size
  config.fb_count = 1;  // Reduce frame buffer count to 1 to save memory

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    ESP.restart();  // Restart ESP32 if camera initialization fails
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  s->set_framesize(s, FRAMESIZE_QVGA);  // Ensure the frame size is set correctly
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi.");
}

void sendText(String text) {
  if (!client.connected()) {
    reconnect();  // Ensure the client is connected
  }
  client.publish(MQTT_PUBLISH_TOPIC, text.c_str());
}

String sendImage() {
  if (!client.connected()) {
    reconnect();  // Ensure the client is connected before sending the image
  }

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    return "Camera capture failed";
  }

  String imageFile = "data:image/jpeg;base64,";
  imageFile += base64::encode((const uint8_t*)fb->buf, fb->len);
  esp_camera_fb_return(fb);

  // Print image size and check buffer size
  Serial.print("Image size: ");
  Serial.println(imageFile.length());

  const size_t chunkSize = 4096;  // Define a larger chunk size
  size_t sentBytes = 0;

  // Send image in chunks
  while (sentBytes < imageFile.length()) {
    String chunk = imageFile.substring(sentBytes, sentBytes + chunkSize);
    Serial.print("Sending chunk: ");
    Serial.println(chunk.length());

    // Retry mechanism
    boolean result = false;
    int retries = 3;
    while (retries > 0) {
      result = client.publish(MQTT_PUBLISH_TOPIC, chunk.c_str());
      if (result) {
        break;
      } else {
        Serial.println("Failed to send chunk, retrying...");
        reconnect();
        retries--;
        delay(100);  // Short delay before retrying
      }
    }

    if (!result) {
      Serial.println("Failed to send chunk after retries");
      return "Failed to send image";
    }

    sentBytes += chunk.length();
    delay(50);  // Short delay to prevent overwhelming the broker
  }

  Serial.println("Image sent successfully");
  return "Image sent successfully";
}

void deserializeCommand(String command) {
  if (command == "ambilfoto") {
    cmd = "ambilfoto";  // Set the command to take a photo
  }
}
