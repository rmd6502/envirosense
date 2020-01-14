#include <CStringBuilder.h>

#include <AceTime.h>

#include <Arduino_JSON.h>

#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <deque>

#include "config.h"

using namespace ace_time;
using namespace ace_time::clock;

static BasicZoneProcessor pacificProcessor;
static NtpClock ntpClock;
SystemClockLoop systemClock(&ntpClock, nullptr /*backup*/);

enum States {
  IDLE,
  HEATING,
  READING,
  RECONNECTING
};

States currentState = IDLE;
uint32_t nextStateTime = 0;

uint32_t transitionDelaysMs[] = {
  10000, 60000, 20000, 5000
};
const uint8_t heater = 4;

const float vcesat = 0.3;
uint32_t maxSleep = ESP.deepSleepMax();
uint32_t sleepTimeUS = 5 /* 60 */* 1e6;

const int NUM_SAMPLES = 32;
std::deque<uint16_t> samples;
uint32_t sample_sum = 0;
int sample_index = 0;

WiFiClient espClient;
HTTPClient client;

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
    Serial.println("failed to connect, we should reset to see if it connects");
    delay(1000);
    ESP.reset();
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

void loop() {
  ArduinoOTA.handle();
  systemClock.loop();
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
    case READING: {
      uint16_t reading = analogRead(A0);
      sample_sum += reading;
      samples.push_back(reading);
      if (samples.size() > NUM_SAMPLES) {
        sample_sum -= samples.front();
        samples.pop_front();
      }
      if (samples.size() == NUM_SAMPLES) {
          double voltage = ((double)sample_sum/samples.size())/1024.0;
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
          
          // Single reading, and an array of readings
          JSONVar reading, readings;
          reading["app_key"] = "app";
          reading["net_key"] = "net";
          reading["device_id"] = "esp8266-mq135-rmd";
          reading["captured_at"] = String(timeBuf);
          JSONVar channels;
          channels["ch1"] = String(voltage,3);
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
      } else {
        delay(50);
        if (millis() < nextStateTime) return;
      }
      Serial.println("Transition to IDLE");
      currentState = IDLE;
      analogWrite(heater, 0);
      ESP.deepSleep(sleepTimeUS, WAKE_RF_DEFAULT);
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
