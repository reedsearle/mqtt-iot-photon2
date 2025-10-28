/* 
 * Project myProject
 * Author: Your Name
 * Date: 
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

#include "Particle.h"
#include <Wire.h>
#include "Button.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "../lib/RotaryEncoder/RotaryEncoder.h"
#include "credentials.h"
#include "application.h"
#include "MQTT.h"
#include "neopixel.h"


struct EAS_Struct {
  int horizontal;
  int vertical;
  bool shake;
};

SYSTEM_MODE(SEMI_AUTOMATIC);

const int DEBUG_LED_PIN = D7;

const int DISPLAY_ADDR = 0x3C;
const int DISPLAY_RESET = -1;
const int DISPLAY_WIDTH = 128;
const int DISPLAY_HEIGHT = 64;

const int HORZ_A_PIN = D8;
const int HORZ_B_PIN = D9;

const int VERT_A_PIN = D2;
const int VERT_B_PIN = D3;

const int HORZ_SW_PIN = D10;
const int VERT_SW_PIN = D5;
const int HORZ_LED_PIN = D6; 
const int VERT_LED_PIN = D4; 

int horzPosNew;
int vertPosNew;

int mqttCount;

bool shake, oldShake;

bool connected;

// Unique client ID per device
String mqttUsername = System.deviceID();
String mqttPassword = MQTT_PASSWORD;
String clientId = mqttUsername;
uint16_t retainFlag = 1;
//Command topic
String pixel_cmd_topic = "bootcamp/" + mqttUsername + "/pixel/cmd";
String pixel_state_topic = "bootcamp/" + mqttUsername + "/pixel";
String struct_topic = "bootcamp/" + String(mqttUsername) + "/struct";
void callback(char* topic, byte* payload, unsigned int length);
void connectToMQTT();
void publishPixelState(const char* state);
void handlePixelCommand(char* message);
uint32_t parseHexColor(char* hex);

EAS_Struct EAS_Data;

// Create encoder instances
RotaryEncoder horzEncoder(HORZ_A_PIN, HORZ_B_PIN, 0, DISPLAY_WIDTH, DISPLAY_WIDTH / 2);
RotaryEncoder vertEncoder(VERT_A_PIN, VERT_B_PIN, 0, DISPLAY_HEIGHT, DISPLAY_HEIGHT / 2);

Button horzButton(HORZ_SW_PIN, TRUE);
Button vertButton(VERT_SW_PIN, TRUE);

Adafruit_SSD1306 display(DISPLAY_RESET);

MQTT client(MQTT_SERVER, MQTT_SERVERPORT, callback);
Adafruit_NeoPixel pixel(1, SPI, WS2812B);


void setup() {
  Serial.begin(9600);
  waitFor(Serial.isConnected, 10000);
  pinMode(DEBUG_LED_PIN, OUTPUT);

  // Initialize NeoPixel
  pixel.begin();
  pixel.setBrightness(50);  // 0-255 (start dim)
  pixel.show();  // Initialize all pixels to 'off'

  //Connect to WiFi without going to Particle Cloud
  WiFi.connect();
  while(WiFi.connecting()) {
    Serial.printf(".");
  }

  // Initialize OLED Display. REMOVE FOR DEMO
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  delay(2000);
  display.clearDisplay();

  connected = false;
  // Initialize MQTT connection
  connectToMQTT();
  // Subscribe to command topics
  client.subscribe(pixel_cmd_topic);
 
  // Initialize encoders (this also attaches interrupts)
  horzEncoder.begin();
  vertEncoder.begin();

  pinMode(HORZ_LED_PIN, OUTPUT);
  pinMode(VERT_LED_PIN, OUTPUT);

  digitalWrite(HORZ_LED_PIN, LOW);
  digitalWrite(VERT_LED_PIN, LOW);

  shake = false;
  oldShake = false;
}

void loop() {
  if(connected) {
    client.loop();
  } else {
    connectToMQTT();
  }

  static int horzPosOld = -999;
  static int vertPosOld = -999;

  if(horzButton.isClicked()) {
    horzPosNew = DISPLAY_WIDTH / 2;
    vertPosNew = DISPLAY_HEIGHT / 2;
    horzEncoder.setPosition(horzPosNew);
    vertEncoder.setPosition(vertPosNew);

    digitalWrite(HORZ_LED_PIN, HIGH);
  } else {
    // Get current positions from encoders
    horzPosNew = horzEncoder.getPosition();
    vertPosNew = vertEncoder.getPosition();

    digitalWrite(HORZ_LED_PIN, LOW);
  }

  if(vertButton.isClicked()) {
    display.clearDisplay();
    display.display();
    digitalWrite(VERT_LED_PIN, HIGH);
    shake = true;
  } else {
    digitalWrite(VERT_LED_PIN, LOW);
    shake = false;
  }

  // Only print if position changed or shaken
  if(horzPosNew != horzPosOld || vertPosNew != vertPosOld || shake != oldShake) {
    horzPosOld = horzPosNew;
    vertPosOld = vertPosNew;
    oldShake = shake;

    EAS_Data.horizontal = horzPosNew;
    EAS_Data.vertical = vertPosNew;
    EAS_Data.shake = shake;

    if(client.isConnected()) {

      String payload = String::format(
        "{\"horizontal\":%d,\"vertical\":%d,\"shake\":%s}",
        EAS_Data.horizontal,
        EAS_Data.vertical,
        EAS_Data.shake ? "true" : "false"
      );

      client.publish(struct_topic, payload);
      mqttCount++;
    }

    Serial.printf("Cursor Position => X: %d, Y: %d\n", horzPosNew, vertPosNew);
    display.drawPixel(horzPosNew, vertPosNew, WHITE);
    display.display();
  }
}

  void callback(char* topic, byte* payload, unsigned int length) {
    // Convert payload to string
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.printf("📩 Received on %s: %s\n", topic, message);

    // Check if this is the pixel command topic
    if (strcmp(topic, pixel_cmd_topic.c_str()) == 0) {
      handlePixelCommand(message);
    }
  }

  // Parse and execute pixel commands
  void handlePixelCommand(char* message) {
    // Option 1: Simple color names
    if (message[0] == '#' && strlen(message) == 7) {
      uint32_t color = parseHexColor(message);
      pixel.setPixelColor(0, color);
      pixel.show();
      publishPixelState(message);
    } else {
      Serial.printf("❌ Unknown pixel command: %s\n", message);
    }
  }

  void publishPixelState(const char* state) {
    client.publish(pixel_state_topic, state, true);  // retained
    Serial.printf("✅ Pixel state: %s\n", state);
  }

  // Parse hex color string "#RRGGBB" to uint32_t
  uint32_t parseHexColor(char* hex) {
    // Skip the '#'
    char* str = hex + 1;

    // Convert hex string to RGB values
    long color = strtol(str, NULL, 16);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    return pixel.Color(r, g, b);
  }

  // Recommended connection pattern for bootcamp
  void connectToMQTT() {
    uint64_t lastTime = 0;
    unsigned long mqttDelay = 1000;
      if(millis() - lastTime > mqttDelay) {
      connected = client.connect(MQTT_SERVER, mqttUsername, MQTT_PASSWORD);
       if (connected) {
        Serial.println("MQTT Connected!");
        pixel.setPixelColor(0, 0x00FF00);
        pixel.show();
        mqttCount = 0;
      } else {
        Serial.printf("MQTT NOT Connected! Try %d\n", mqttCount++);
        pixel.setPixelColor(0, 0xFF0000);
        pixel.show();
      }
      lastTime = millis();
    } 
    // TODO - add subscriptions here
    // client.subcribe();

  }
