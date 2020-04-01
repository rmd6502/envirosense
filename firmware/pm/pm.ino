#include <CStringBuilder.h>

#define ACE_TIME_NTP_CLOCK_DEBUG 1
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

#define PIN_LED           2
#define LED_ON      HIGH
  #define LED_OFF     LOW  
#define SP30_COMMS I2C_COMMS
#define DEBUG 0

enum States {
  IDLE,
  READING,
  RECONNECTING,
  ERRORSTATE
};

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

States currentState = IDLE;
uint32_t nextStateTime = 0;

uint32_t transitionDelaysMs[] = {
  60000, 20000, 5000, 99999
};
// Transistor to control power to the sensor
const uint8_t sensor = 4;

const float vcesat = 0.3;
uint32_t sleepTimeUS = 10 * 60 * 1e6;

const int NUM_SAMPLES = 32;
std::deque<uint16_t> samples;
uint32_t sample_sum = 0;
int sample_index = 0;

WiFiClient espClient;
HTTPClient client;
SPS30 sps30;

void setup() {
  // put your setup code here, to run once:
  pinMode(sensor, OUTPUT);
  Serial.begin(115200);
  Serial.println("starting wifiManager");
  
  // Now go back to our normal connection.
  WifiConnect();
  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  ntpClock.setup();

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

    // set driver debug level
    sps30.EnableDebugging(DEBUG);
    if (sps30.begin(SP30_COMMS) == false) {
      Serial.println("could not initialize communication channel.");
      currentState = ERRORSTATE;
    }
    if (sps30.probe() == false) {
      Serial.println("could not probe / connect with SPS30.");
      currentState = ERRORSTATE;
    }
    else
      Serial.println(F("Detected SPS30."));

}

void loop() {
  ArduinoOTA.handle();
  systemClock.loop();
  if (millis() < nextStateTime && currentState != READING) return;
  switch (currentState) {
    case IDLE:
      Serial.println("Transition to Reading");
      if (sps30.start() == true)
        Serial.println(F("Measurement started"));
      else {
        Serial.println("Could NOT start measurement");
      currentState = ERRORSTATE;
    }
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
      // Single reading, and an array of readings
      JSONVar reading, readings;
      reading["app_key"] = "app";
      reading["net_key"] = "net";
      reading["device_id"] = "esp32-sps30-rmd";
      reading["captured_at"] = String(timeBuf);
      JSONVar channels;
      sps_values *val = read_all();
      if (val != nullptr) {
        channels["ch1"] = String(val->MassPM1);
        channels["ch2"] = String(val->MassPM2);
        channels["ch3"] = String(val->MassPM4);
        channels["ch4"] = String(val->MassPM10);
        channels["ch5"] = String(val->NumPM0);
        channels["ch6"] = String(val->NumPM1);
        channels["ch7"] = String(val->NumPM2);
        channels["ch8"] = String(val->NumPM4);
        channels["ch9"] = String(val->NumPM10);
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
      }
      
      Serial.print("Transition to IDLE, deep sleep for");
      Serial.print(sleepTimeUS);
      Serial.println("us");
      currentState = IDLE;
      esp_wifi_stop();
      sps30.stop();
      esp_deep_sleep(sleepTimeUS);
      break;
    }
    case RECONNECTING:
      Serial.println("transition to reading");
      currentState = READING;
      break;
    case ERRORSTATE:
      break;
  }
  nextStateTime = millis() + transitionDelaysMs[currentState];
  Serial.print("current state "); Serial.println(currentState);
  Serial.print("nextStateTime "); Serial.println(nextStateTime);
  Serial.print("current millis "); Serial.println(millis());
}

/**
 * @brief : read and display all values
 */
sps_values* read_all()
{
  static bool header = true;
  uint8_t ret, error_cnt = 0;
  static struct sps_values val;

  // loop to get data
  do {

    ret = sps30.GetValues(&val);

    // data might not have been ready
    if (ret == ERR_DATALENGTH){

        if (error_cnt++ > 3) {
          ErrtoMess("Error during reading values: ",ret);
          return(nullptr);
        }
        delay(1000);
    }

    // if other error
    else if(ret != ERR_OK) {
      ErrtoMess("Error during reading values: ",ret);
      return(nullptr);
    }

  } while (ret != ERR_OK);

  // only print header first time
  if (header) {
    Serial.println(F("-------------Mass -----------    ------------- Number --------------   -Average-"));
    Serial.println(F("     Concentration [μg/m3]             Concentration [#/cm3]             [μm]"));
    Serial.println(F("P1.0\tP2.5\tP4.0\tP10\tP0.5\tP1.0\tP2.5\tP4.0\tP10\tPartSize\n"));
    header = false;
  }

  Serial.print(val.MassPM1);
  Serial.print(F("\t"));
  Serial.print(val.MassPM2);
  Serial.print(F("\t"));
  Serial.print(val.MassPM4);
  Serial.print(F("\t"));
  Serial.print(val.MassPM10);
  Serial.print(F("\t"));
  Serial.print(val.NumPM0);
  Serial.print(F("\t"));
  Serial.print(val.NumPM1);
  Serial.print(F("\t"));
  Serial.print(val.NumPM2);
  Serial.print(F("\t"));
  Serial.print(val.NumPM4);
  Serial.print(F("\t"));
  Serial.print(val.NumPM10);
  Serial.print(F("\t"));
  Serial.print(val.PartSize);
  Serial.print(F("\n"));

  return &val;
}

/**
 *  @brief : display error message
 *  @param mess : message to display
 *  @param r : error code
 *
 */
void ErrtoMess(char *mess, uint8_t r)
{
  char buf[80];

  Serial.print(mess);

  sps30.GetErrDescription(r, buf, 80);
  Serial.println(buf);
}

void WifiConnect()
{
  //Local intialization. Once its business is done, there is no need to keep it around
  ESP_WiFiManager ESP_wifiManager;
  String ssid;

  // We can't use WiFi.SSID() in ESP32as it's only valid after connected. 
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();
  
  //Remove this line if you do not want to see WiFi password printed
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  // SSID to uppercase 
  ssid.toUpperCase();
  
  if (Router_SSID == "")
  {
    Serial.println("We haven't got any access point credentials, so get them now");   
     
    digitalWrite(PIN_LED, LED_ON); // Turn led on as we are in configuration mode.
    
    //it starts an access point 
    //and goes into a blocking loop awaiting configuration
    if (!ESP_wifiManager.startConfigPortal(apName, apPass)) 
      Serial.println("Not connected to WiFi but continuing anyway.");
    else 
      Serial.println("WiFi connected...yeey :)");    
  }

  digitalWrite(PIN_LED, LED_OFF); // Turn led off as we are not in configuration mode.
  
  #define WIFI_CONNECT_TIMEOUT        30000L
  #define WHILE_LOOP_DELAY            200L
  #define WHILE_LOOP_STEPS            (WIFI_CONNECT_TIMEOUT / ( 3 * WHILE_LOOP_DELAY ))
  
  uint32_t startedAt = millis();
  
  while ( (WiFi.status() != WL_CONNECTED) && (millis() - startedAt < WIFI_CONNECT_TIMEOUT ) )
  {   
    WiFi.mode(WIFI_STA);
    WiFi.persistent (true);
    // We start by connecting to a WiFi network
  
    Serial.print("Connecting to ");
    Serial.println(Router_SSID);
  
    WiFi.begin(Router_SSID.c_str(), Router_Pass.c_str());

    int i = 0;
    while((!WiFi.status() || WiFi.status() >= WL_DISCONNECTED) && i++ < WHILE_LOOP_STEPS)
    {
      delay(WHILE_LOOP_DELAY);
    }    
  }

  Serial.print("After waiting ");
  Serial.print((millis()- startedAt) / 1000);
  Serial.print(" secs more in setup(), connection result is ");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
    ESP_wifiManager.startConfigPortal(apName, apPass);
  }
}
