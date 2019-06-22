// #include <Arduino.h>
#define BLYNK_PRINT Serial
// #include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include "SPIFFS.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ThingSpeak.h>
#include <esp_sleep.h>
//#include <Temperature_LM75_Derived.h>
#include <wire.h>
#include "SSD1306.h"
//Generic_LM75 temperature;

// Pin definitions for I2C
#define OLED_SDA 26 // pin 26
#define OLED_SDC 25 // pin 25
#define OLED_ADDR 0x78

SSD1306 display(OLED_ADDR, OLED_SDA, OLED_SDC); // For I2C

esp_sleep_wakeup_cause_t wakeup_reason;

unsigned long myChannelNumber = 38484;
const char *myWriteAPIKey = "UW9T0WNPQPVY7QPB";
WiFiClient client;

char blynk_token[33] = "591b947a24354dd085ef3ae6d7ffa399";

const int led = 19;
const int pin_adc_1 = 35; //GPIO usado para captura analógica
const int pin_adc_2 = 32; //GPIO usado para captura analógica
uint16_t n = 0;
bool bomba = 0;
bool bomba_desl = 0;
// String myStatus = "";

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 60       /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR int bootCount = 0;

//flag for saving data
bool shouldSaveConfig = false;

void Atualiza_display(void)
{
  //ESP.wdtFeed();
  //v_int = ESP.getVcc(); //pega a tensao interna
  //String ip_m = String(WiFi.localIP());
  //String ip_m = String(ip);
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  //display.drawString(0, 0, time_url);
  //display.drawString(0, 26, ip_m);
  //display.setFont(ArialMT_Plain_16);
  //display.drawString(0, 15, "URL: " + String(falha_url_time));
  //display.drawString(0, 15, "URL: " + String(falha_url_time));  //imprime o numero de falha_url_times de tentativas de pegar hora no servidor
  //display.drawString(0, 15, relogio_na_atualizacao);
  if (WiFi.status() != WL_CONNECTED)
  {
    display.drawString(70, 15, "Wifi OFF");
  }
  else
    display.drawString(70, 15, "Wifi ON");
  //display.drawString(70, 15, "V: " + String(v_int));
  //display.drawString(0, 20, time_url);
  //display.setFont(ArialMT_Plain_16);
  //display.drawString(110, 40, String(n_falha_url_time));  //apresenta o numero de erros de conexoes
  //display.setFont(ArialMT_Plain_24);
  //display.setFont(Bitstream_Charter_Plain_30);
  //display.drawString(0, 30, time_atual);
  display.display();
  //yield(); //um ciclo sem fazer nada e dar tempo para o processador. evita traver no wifi
}

//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void publica_web();

void IRAM_ATTR bomba_desligou()
{
  detachInterrupt(15);
  bomba = 0;
  bomba_desl = 1;
  Serial.print("Bomba: ");
  Serial.println(bomba);  
}

void print_wakeup_reason()
{
  //String reason = "";
  wakeup_reason = esp_sleep_get_wakeup_cause(); //recupera a causa do despertar
  Serial.print("Motivo do wake up: ");
  Serial.println(wakeup_reason);
  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0:{
      Serial.println("Wakeup caused by external signal using RTC_IO");
      bomba=1;
      //ESP_ERROR_CHECK_WITHOUT_ABORT(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0));
      //esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
      //ESP_ERROR_CHECK_WITHOUT_ABORT(rtc_gpio_deinit(GPIO_NUM_2));
      break;
    }
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

void setup()
{
  delay(100);
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  bootCount++;
  Serial.begin(9600);
  Serial.println("");
  //Wire.begin();

  ///////// Display 
  display.init();
  //display.flipScreenVertically();
  //display.setContrast(255);
  //display.clear();
  //display.setTextAlignment(TEXT_ALIGN_LEFT);
  //display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Iniciando Wifi...");
  display.display();

  //função para imprimir a causa do ESP32 despertar
  print_wakeup_reason();
  pinMode(2, INPUT_PULLDOWN);
  pinMode(15, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(15), bomba_desligou, FALLING);
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
        }
        else
        {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
  //end read

  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 33);
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);  //Se o portal não conectar em 180 s, reseta

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  // wifiManager.resetSettings();      //limpa todos os wifi salvos para testar o portal

  wifiManager.addParameter(&custom_blynk_token);
  Blynk.config(custom_blynk_token.getValue());

  if (!wifiManager.autoConnect("ESP32"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  //read updated parameters
  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
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
}

void publica_web()
{
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
  if(bomba==1)  Blynk.virtualWrite(V4, 255); //manda o estado da bomba
  else Blynk.virtualWrite(V4, 0); //manda o estado da bomba

  //ThingSpeak
  ThingSpeak.setField(1, tensao_1);
  ThingSpeak.setField(2, tensao_2);
  ThingSpeak.setField(3, bootCount);
  ThingSpeak.setField(4, bomba);
  //ThingSpeak.setField(4, temp);
  // set the status
  ThingSpeak.setStatus("ONLINE");

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200)
  {
    Serial.println("Channel thingspeak update successful.");
  }
  else
  {
    Serial.println("Problem updating channel Thingspeak. HTTP error code " + String(x));
  }
}

void loop()
{
  delay(250);
  publica_web();
  digitalWrite(led, LOW);
  if (bomba == 0)
  {    
    Serial.println("Bomba não foi ligada. Indo dormir");
    esp_deep_sleep_start(); //se a bomba nao foi ligada, pode ir domir
  }
  while (bomba_desl==0)
  {
    digitalWrite(led, HIGH);
    delay(250);
    digitalWrite(led, LOW);    
    delay(250);
  }
  publica_web();
  Serial.print("Bomba desligou e foi dormir");
  esp_deep_sleep_start(); //se a bomba desligou pode ir domir
  // int state = digitalRead(LED_BUILTIN);
  // digitalWrite(LED_BUILTIN, !state);
}