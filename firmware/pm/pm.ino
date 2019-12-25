#include <CStringBuilder.h>

#include <AceTime.h>

#include <Arduino_JSON.h>

#include <ESP_WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <deque>
#include <sps30.h>

#include "config.h"

using namespace ace_time;
using namespace ace_time::clock;

static BasicZoneProcessor pacificProcessor;
static NtpClock ntpClock;
SystemClockLoop systemClock(&ntpClock, nullptr /*backup*/);

#define SP30_COMMS I2C_COMMS
#define DEBUG 0

enum States {
  IDLE,
  READING,
  RECONNECTING
};

States currentState = IDLE;
uint32_t nextStateTime = 0;

uint32_t transitionDelaysMs[] = {
  60000, 20000, 5000
};
// Transistor to control power to the sensor
const uint8_t sensor = 4;

const float vcesat = 0.3;
uint32_t sleepTimeUS = 5 /* 60 */* 1e6;

const int NUM_SAMPLES = 32;
std::deque<uint16_t> samples;
uint32_t sample_sum = 0;
int sample_index = 0;

WiFiClient espClient;
HTTPClient client;

void setup() {
  // put your setup code here, to run once:
  pinMode(sensor, OUTPUT);
  Serial.begin(115200);
  
  // Now go back to our normal connection.
  ESP_WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  if (!wifiManager.autoConnect(apName, apPass)) {
    Serial.println("failed to connect, we should reset to see if it connects");
    delay(1000);
    esp_restart();
    delay(5000);
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  ntpClock.setup(nullptr, nullptr);

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
      Serial.print("Progress: ");
      Serial.print(progress / (total / 100));
      Serial.println("%");
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.print("Error");
      Serial.print(error);
      Serial.print(": ");
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  systemClock.loop();
  if (millis() < nextStateTime && currentState != READING) return;
  switch (currentState) {
    case IDLE:
      Serial.println("Transition to Heating");
      currentState = READING;
      break;
    case READING: {
      
    //      Serial.print("reading ");
    //      Serial.println(reading);
    /*
     * [
          {
            "app_key": "app",
            "net_key": "net",
            "device_id": "esp8266-mq135-rmd",
            "latitude": 29.774975,
            "longitude":  -95.351613,
            "captured_at": "2019-08-22 21:03:03 -0700",
            "channels": {
              "ch1": 0.414001
            }
           },
          
          {
            "app_key": "app",
            "net_key": "net",
            "device_id": "esp8266-mq135-rmd",
            "latitude": 29.774975,
            "longitude":  -95.351613,
            "captured_at": "2019-08-22 21:05:03 -0700",
            "channels": {
              "ch1": 0.514001
            }
           }
        ]
     */
      acetime_t epochSeconds = systemClock.getNow();
      auto pacificTz = TimeZone::forZoneInfo(&zonedb::kZoneAmerica_Los_Angeles,
        &pacificProcessor);
      auto pacificTime = ZonedDateTime::forEpochSeconds(epochSeconds, pacificTz);
      char timeBuf[100];
      CStringBuilder timePrint(timeBuf, sizeof(timeBuf));
      pacificTime.printTo(timePrint);
      double pm;
      // Single reading, and an array of readings
      JSONVar reading, readings;
      reading["app_key"] = "app";
      reading["net_key"] = "net";
      reading["device_id"] = "esp32-sps30-rmd";
      reading["captured_at"] = String(timeBuf);
      JSONVar channels;
      channels["ch1"] = String(pm,3);
      reading["channels"] = channels;
      readings[0] = reading;
      Serial.print("sending bytes "); Serial.println(JSON.stringify(readings));
      if (client.begin(espClient, apiServer)) {
        client.addHeader("Content-Type", "application/json");
        int status = client.POST(JSON.stringify(readings));
        Serial.print("send status "); Serial.println(status);
      } else {
        Serial.println("Failed to connect to server");
      }
      
      Serial.println("Transition to IDLE");
      currentState = IDLE;
      ESP.deepSleep(sleepTimeUS);
      break;
    }
    case RECONNECTING:
      Serial.println("transition to reading");
      currentState = READING;
      break;
  }
  nextStateTime = millis() + transitionDelaysMs[currentState];
  Serial.print("nextStateTime "); Serial.println(nextStateTime);
  Serial.print("current millis "); Serial.println(millis());
}
