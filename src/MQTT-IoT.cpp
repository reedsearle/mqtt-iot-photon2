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
String clientId = "photon2-" + System.deviceID();
//Command topic
String cmd_topic = "bootcamp/" + String(MQTT_USERNAME) + "/+/command";
// Last Will message
String lwt_topic = "bootcamp/" + String(MQTT_USERNAME) + "/status";
void callback(char* topic, byte* payload, unsigned int length);
void connectToMQTT();

EAS_Struct EAS_Data;

// Create encoder instances
RotaryEncoder horzEncoder(HORZ_A_PIN, HORZ_B_PIN, 0, DISPLAY_WIDTH, DISPLAY_WIDTH / 2);
RotaryEncoder vertEncoder(VERT_A_PIN, VERT_B_PIN, 0, DISPLAY_HEIGHT, DISPLAY_HEIGHT / 2);

Button horzButton(HORZ_SW_PIN, TRUE);
Button vertButton(VERT_SW_PIN, TRUE);

Adafruit_SSD1306 display(DISPLAY_RESET);

MQTT client(MQTT_SERVER, MQTT_SERVERPORT, callback);

void setup() {
  Serial.begin(9600);
  waitFor(Serial.isConnected, 10000);
  pinMode(DEBUG_LED_PIN, OUTPUT);

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
  // Publish online status (retained)
  client.publish(lwt_topic, "online", true);
  // Subscribe to command topics
  client.subscribe(cmd_topic);
 
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

      client.publish("demo/" + String(MQTT_USERNAME) + "/struct", payload);
      mqttCount++;
    }

    Serial.printf("Cursor Position => X: %d, Y: %d\n", horzPosNew, vertPosNew);
    display.drawPixel(horzPosNew, vertPosNew, WHITE);
    display.display();
  }
}

  void callback(char* topic, byte* payload, unsigned int length) {
      char msg[length + 1];
      memcpy(msg, payload, length);
      msg[length] = '\0';

      Serial.printf("Message arrived on %s: %s\n", topic, msg);

      // Handle LED control
      if (strcmp(topic, "bootcamp/" + String(MQTT_USERNAME) + "/led/command") == 0) {
          if (strcmp(msg, "ON") == 0) {
              digitalWrite(D7, HIGH);
          } else {
              digitalWrite(D7, LOW);
          }
      }
  }

  // Recommended connection pattern for bootcamp
  void connectToMQTT() {
      // Unique client ID per device
      String clientId = "photon2-" + System.deviceID();

      // Attempt connection with LWT
      // bool connected = client.connect(MQTT_SERVER, MQTT_USERNAME, MQTT_PASSWORD, lwt_topic, MQTT::QOS1, true, "offline");
      // bool connected = client.connect(MQTT_SERVER, MQTT_USERNAME, MQTT_PASSWORD);
      // bool connected = client.connect(MQTT_SERVER);
      // if (connected) {
      //   Serial.println("MQTT Connected!");
      // } else {
      //   Serial.println("MQTT NOT Connected!");
      // }
      uint64_t lastTime = 0;
      unsigned long mqttDelay = 1000;
      do{
        if(millis() - lastTime > mqttDelay) {
        connected = client.connect(MQTT_SERVER);
          if (connected) {
            Serial.println("MQTT Connected!");
          } else {
            Serial.printf("MQTT NOT Connected! Try %d\n", mqttCount++);
          }
          lastTime = millis();
        } 
      }while(!connected);
      // TODO - add subscriptions here
      // client.subcribe();
      mqttCount = 0;

  }
