#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include "config.h"

enum States {
  IDLE,
  HEATING,
  READING
};

States currentState = IDLE;
uint32_t nextStateTime = 0;

uint32_t transitionDelaysMs[] = {
  0, 60000, 20000
};
const uint8_t heater = 4;

const float vcesat = 0.3;
uint32_t maxSleep = ESP.deepSleepMax();
uint32_t sleepTimeUS = 60 * 1e6;

WiFiClient espClient;
PubSubClient client(espClient);

const String topic = "/c6h14";

void setup() {
  // put your setup code here, to run once:
  pinMode(A0, INPUT);
  pinMode(heater, OUTPUT);
  analogWrite(heater, 0);
  Serial.begin(115200);
  analogWriteFreq(4000);
  analogWriteRange(1023);
  Serial.print("max deep sleep ");
  Serial.println(maxSleep);
  
  // Workaround since the wifi seems to be powered down after deep sleep.
  WiFi.forceSleepWake();
  delay(5000);
  // Now go back to our normal connection.
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  if (!wifiManager.autoConnect(apName, apPass)) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(1000);
    ESP.reset();
    delay(5000);
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqttServer, 1883);

  ArduinoOTA.setHostname(host);

   ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    if (sleepTimeUS > maxSleep) {
      sleepTimeUS = maxSleep;
    }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      return;
    }
  }
}

void loop() {
  ArduinoOTA.handle();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if (millis() < nextStateTime && currentState != READING) return;
  switch (currentState) {
    case IDLE:
      Serial.println("Transition to Heating");
      currentState = HEATING;
      analogWrite(heater, 1023);
      break;
    case HEATING:
      Serial.println("Transition to READING");
      currentState = READING;
      break;
    case READING:
      uint16_t reading = analogRead(A0);
//      Serial.print("reading ");
//      Serial.println(reading);
      DynamicJsonDocument json(1024);
      json["voltage"] = ((double)reading)/1024.0;
      if (client.connected()) {
        client.beginPublish(topic.c_str(), measureJson(json), true);
        serializeJson(json, client);
        client.endPublish();
      }
      delay(50);
      if (millis() < nextStateTime) return;
      Serial.println("Transition to IDLE");
      currentState = IDLE;
      analogWrite(heater, 0);
      ESP.deepSleep(sleepTimeUS, WAKE_RF_DEFAULT);
      break;
  }
  nextStateTime = millis() + transitionDelaysMs[currentState];
}
