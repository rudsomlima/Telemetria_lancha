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
#include <Temperature_LM75_Derived.h>
#include "driver/rtc_io.h"
Generic_LM75 temperature;

esp_sleep_wakeup_cause_t wakeup_reason;

unsigned long myChannelNumber = 38484;
const char * myWriteAPIKey = "UW9T0WNPQPVY7QPB";
WiFiClient  client;

char blynk_token[33] = "591b947a24354dd085ef3ae6d7ffa399";

const int led = 19;
const int pin_adc_1 = 35; //GPIO usado para captura analógica
const int pin_adc_2 = 32; //GPIO usado para captura analógica
uint16_t n=0;
uint8_t bomba=0;
// String myStatus = "";

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR int bootCount = 0;

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void publica_web();

void IRAM_ATTR bomba_ligou()
{
  //detachInterrupt(2);
  bomba=0;
  Serial.print("Bomba: ");
  Serial.println(bomba);
  Serial.print("Bomba desligou e foi dormir");
  esp_deep_sleep_start();   //se a bomba desligou pode ir domir
}

void print_wakeup_reason(){
  
  //String reason = "";
  wakeup_reason = esp_sleep_get_wakeup_cause(); //recupera a causa do despertar
  Serial.print("Motivo do wake up: ");
  Serial.println(wakeup_reason);
  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : {
      Serial.println("Wakeup caused by external signal using RTC_IO");
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0));
      //esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
      ESP_ERROR_CHECK_WITHOUT_ABORT(rtc_gpio_deinit(GPIO_NUM_2));

      break;
      }
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void setup() {
  delay(1000);
  pinMode(led, OUTPUT);
  pinMode(2, INPUT_PULLUP);
  digitalWrite(led,HIGH);
  bootCount++;
  Serial.begin(9600);
  Serial.println("");
  Wire.begin();

  //função para imprimir a causa do ESP32 despertar
  print_wakeup_reason();
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), bomba_ligou, FALLING);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1); //1 = High, 0 = Low //GPIO 02 //volta a habilitar a int da bomba
  

    //read configuration from FS json
    Serial.println("mounting FS...");

    if (SPIFFS.begin())
    {
      Serial.println("mounted file system");
      if (SPIFFS.exists("/config.json"))
      {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile)
        {
          Serial.println("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);

          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBuffer;
          JsonObject &json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success())
          {
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

  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 33);
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  // wifiManager.resetSettings();      //limpa todos os wifi salvos para testar o portal

  wifiManager.addParameter(&custom_blynk_token);
  Blynk.config(custom_blynk_token.getValue());

  if (!wifiManager.autoConnect("ESP32")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  //read updated parameters
  strcpy(blynk_token, custom_blynk_token.getValue());

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

  Serial.print("local ip: ");
  Serial.println(WiFi.localIP());
  Serial.println("ESP conectado no WIFI !");
  adcAttachPin(pin_adc_1);
  adcAttachPin(pin_adc_2);
  // analogSetClockDiv(255); // 1338mS
	// adcStart(pin_adc);
	analogReadResolution(10); // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
	// analogSetAttenuation(ADC_6db); // Default is 11db which is very noisy. Recommended to use 2.5 or 6.
  ThingSpeak.begin(client);
  Blynk.config(blynk_token);  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

            // Stop transmitting
}

void publica_web() {
  int bat_int = analogRead(pin_adc_1);
  int bat_12v = analogRead(pin_adc_2);
  float tensao_1 = bat_int * 3.3 / 1024;
  float tensao_2 = bat_12v * 3.3 / 1024;
  // // print out the value you read:
  Serial.print(bootCount);
  Serial.print(" - bat_int: ");
  Serial.println(tensao_1);
  Serial.print(n);
  Serial.print(" - bat_12v: ");
  Serial.println(tensao_2);
  Serial.print("Bomba: ");
  Serial.println(bomba);
  //Serial.print("Temperature = ");
  //float temp = temperature.readTemperatureC();
  //Serial.print(temp);
  //Serial.println(" C");
  //Serial.print("SDA: ");
  //Serial.println(SDA);
  //Serial.print("SCL: ");
  //Serial.println(SCL);
  //Serial.print("Clock: ");
  //Serial.println(Wire.getClock());

  //Blynk
  Blynk.connect();
  Blynk.virtualWrite(V1, tensao_1);
  Blynk.virtualWrite(V2, tensao_2);
  Blynk.virtualWrite(V3, bootCount);
  //Blynk.virtualWrite(V4, temp);
  if (bomba==1) //se acordou por causa da interrupção e a bomba está ligada
  {                            
    Blynk.virtualWrite(V4, 1); //manda 1 indicando bomba ligada
    ThingSpeak.setField(4, 1);
  }
  else
  {
    Blynk.virtualWrite(V4, 0); //manda 1 indicando bomba desligada
    ThingSpeak.setField(4, 0);
  }
  //ThingSpeak
  ThingSpeak.setField(1, tensao_1);
  ThingSpeak.setField(2, tensao_2);
  ThingSpeak.setField(3, bootCount);
  //ThingSpeak.setField(4, temp);
  // set the status
  ThingSpeak.setStatus("ONLINE");

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200)
  {
    Serial.println("Channel update successful.");
  }
  else
  {
    Serial.println("Problem updating channel Thingspeak. HTTP error code " + String(x));
  }
}

void loop() {
  delay(500);
  publica_web();  
  digitalWrite(led,LOW);
  if(bomba==0) {
    Serial.println("Bomba não foi ligada. Indo dormir");
    esp_deep_sleep_start(); //se a bomba nao foi ligada, pode ir domir
    }
  delay(500);
  digitalWrite(led, HIGH);

  // int state = digitalRead(LED_BUILTIN);
  // digitalWrite(LED_BUILTIN, !state);
}
