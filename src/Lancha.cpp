// #include <Arduino.h>
#define BLYNK_PRINT Serial
// #include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiManager.h>       //https://github.com/tzapu/WiFiManager
#include <ThingSpeak.h>
#include <esp_sleep.h>



unsigned long myChannelNumber = 38484;
const char * myWriteAPIKey = "*************";
WiFiClient  client;


char blynk_token[34] = "********************";

const int led = 19;
const int pin_adc_1 = 35; //GPIO usado para captura analógica
const int pin_adc_2 = 32; //GPIO usado para captura analógica
uint16_t n=0;
// String myStatus = "";

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR int bootCount = 0;

// void configModeCallback (WiFiManager *myWiFiManager) {
//   Serial.println("Entered config mode");
//   Serial.println(WiFi.softAPIP());
//   //if you used auto generated SSID, print it
//   Serial.println(myWiFiManager->getConfigPortalSSID());
//   //entered config mode, make led toggle faster
// }

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  pinMode(led, OUTPUT);
  digitalWrite(led,HIGH);
  bootCount++;
  Serial.begin(115200);

  //read configuration from FS json
  //clean FS, for testing
  // SPIFFS.format();
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(blynk_token, json["blynk_token"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 34);

  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  // wifiManager.resetSettings();      //limpa todos os wifi salvos para testar o portal

  //set static ip
  // wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  wifiManager.addParameter(&custom_blynk_token);

  if (!wifiManager.autoConnect("ESP32", "smolder79")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  //read updated parameters
  strcpy(blynk_token, custom_blynk_token.getValue());
  // wifiManager.autoConnect("ESP32");
  // Serial.println(custom_blynk_token.getValue());
  // Blynk.config(custom_blynk_token.getValue());
  // Blynk.begin(blynk_token, "Virus_online", "smolder79");

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println("ESP conectado no WIFI !");
  adcAttachPin(pin_adc_1);
  adcAttachPin(pin_adc_2);
  // analogSetClockDiv(255); // 1338mS
	// adcStart(pin_adc);
	analogReadResolution(10); // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
	// analogSetAttenuation(ADC_6db); // Default is 11db which is very noisy. Recommended to use 2.5 or 6.
  // pinMode(pin_adc, INPUT); //Pino utilizado para captura analógica
  ThingSpeak.begin(client);
  // esp_deep_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}

void loop() {
  int bat_int = analogRead(pin_adc_1);
  int bat_12v = analogRead(pin_adc_2);
  float tensao_1 = bat_int*3.3/1024;
  float tensao_2 = bat_12v*3.3/1024;
  // // print out the value you read:
  Serial.print(bootCount);
  Serial.print(" - bat_int: ");
  Serial.println(tensao_1);
  Serial.print(n);
  Serial.print(" - bat_12v: ");
  Serial.println(tensao_2);

  //Blynk
  Blynk.virtualWrite(V1, tensao_1);
  Blynk.virtualWrite(V2, tensao_2);
  Blynk.virtualWrite(V3, bootCount);

  //ThingSpeak
  ThingSpeak.setField(1, tensao_1);
  ThingSpeak.setField(2, tensao_2);
  ThingSpeak.setField(3, bootCount);
  // set the status
  ThingSpeak.setStatus("ONLINE");

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
  digitalWrite(led,LOW);
  esp_deep_sleep_start ();

  // int state = digitalRead(LED_BUILTIN);
  // digitalWrite(LED_BUILTIN, !state);
}
