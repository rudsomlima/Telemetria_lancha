// #include <Arduino.h>
#define BLYNK_PRINT Serial
// #include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <Wire.h>
#include "SPIFFS.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ThingSpeak.h>
#include <esp_sleep.h>
//#include <Temperature_LM75_Derived.h>
//#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerif12pt7b.h>

#include <WidgetRTC.h>  //Tem que estar habilitado o RTC no app Blynk do celular
WidgetRTC rtc;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Generic_LM75 temperature;

esp_sleep_wakeup_cause_t wakeup_reason;

unsigned long myChannelNumber = 38484;
const char *myWriteAPIKey = "UW9T0WNPQPVY7QPB";
WiFiClient client;
String ssid;

char blynk_token[33] = "591b947a24354dd085ef3ae6d7ffa399";

const int led = 19;
const int pin_adc_1 = 35; //GPIO usado para captura analógica
const int pin_adc_2 = 32; //GPIO usado para captura analógica
const int interrupcao_desl = 17; //pino de interrupcao para desligar bomba
#define I2C_SDA 23
#define I2C_SCL 22
uint16_t n = 0;
bool bomba = 0; bool bomba_desl = 0; bool flag_toque=0;
float tensao_painel;
float tensao_bateria;
float fator_painel = 9.43;
float fator_bateria = 1;
String currentTime;
String currentDate;
bool flag_atualiza_data;

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 60       /* Time ESP32 will go to sleep (in seconds) */
    RTC_DATA_ATTR uint16_t bootCount = 0;

void Atualiza_data(void)
{
  currentTime = String(hour()) + ":" + minute();
  currentDate = String(day()) + "/" + month() + "/" + year();
  Serial.print("Current time: ");
  Serial.print(currentTime);
  Serial.print(" ");
  Serial.print(currentDate);
  Serial.println();
}

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void IRAM_ATTR bomba_desligou(void)
{
  detachInterrupt(interrupcao_desl);
  bomba = 0;
  bomba_desl = 1;
  Serial.print("Bomba: ");
  Serial.println(bomba);  
}

void IRAM_ATTR touch(void)
{
  touch_pad_intr_disable();
 //detachInterrupt(13); //pino 13 é o touch T4
  flag_toque = 1;
  Serial.println("__________HOUVE TOQUE NO PIN 13");
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
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.begin(9600);
  Serial.println("");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Display static text
  display.println("LIGANDO...");
  display.display();

  //delay(10000);
  //Wire.begin();

  //função para imprimir a causa do ESP32 despertar
  print_wakeup_reason();
  pinMode(2, INPUT_PULLDOWN);
  pinMode(interrupcao_desl, INPUT_PULLDOWN);
  touchAttachInterrupt(T4, touch, 80);
  attachInterrupt(digitalPinToInterrupt(interrupcao_desl), bomba_desligou, FALLING);
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
  ssid = WiFi.SSID();
  //display.println(WiFi.localIP());
  //display.display();
  adcAttachPin(pin_adc_1);
  adcAttachPin(pin_adc_2);
  // analogSetClockDiv(255); // 1338mS
  // adcStart(pin_adc);
  analogReadResolution(10); // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
  analogSetAttenuation(ADC_6db); // Default is 11db which is very noisy. Recommended to use 2.5 or 6.
  ThingSpeak.begin(client);
  Blynk.config(blynk_token);
  Blynk.connect();

  setSyncInterval(1); //tempo de sincronização
  rtc.begin();
  Blynk.syncAll();
  while (year()==1970)  {
    Serial.print(year());
    delay(500);
  }
  Atualiza_data();
  setSyncInterval(10000); //tempo de sincronização

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);  
}

void publica_blink() {  
  Blynk.virtualWrite(V1, tensao_painel);
  Blynk.virtualWrite(V2, tensao_bateria);
  Blynk.virtualWrite(V3, bootCount);
  //Blynk.virtualWrite(V4, temp);
  if (bomba == 1)
    Blynk.virtualWrite(V4, 255); //manda o estado da bomba
  else
    Blynk.virtualWrite(V4, 0); //manda o estado da bomba
}

void publica_thingspeak()
{
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

  //ThingSpeak
  ThingSpeak.setField(1, tensao_painel);
  ThingSpeak.setField(2, tensao_bateria);
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

void leituras() {
  int v_painel=0;
  int bat_12v=0;
  for(n=0;n<100;n++) {
    v_painel = analogRead(pin_adc_1) + v_painel;
    bat_12v = analogRead(pin_adc_2) + bat_12v;    
  }
  tensao_painel = (fator_painel * v_painel * 3.3 / 1024) / 100;
  tensao_bateria = (fator_bateria * bat_12v * 3.3 / 1024) / 100;

  // // print out the value you read:
  //Serial.print(bootCount);
  //Serial.print(" - tensao_painel: ");
  Serial.print(tensao_painel);
  Serial.print("\t");
  //Serial.print(bootCount);
  //Serial.print(" - tensao_bateria: ");
  Serial.println(tensao_bateria);
  //Serial.print("Bomba: ");
  //Serial.println(bomba);
  //long rssi = WiFi.RSSI();
  //Serial.println(rssi);
}

void mostra_display() {
  delay(100);
  display.clearDisplay();
  display.setFont();  
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(currentTime+" "+currentDate);
  if (flag_atualiza_data) {
    display.print(" *");
  }
  else display.print(" ");
  display.setFont(&FreeSerif12pt7b);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 32);
  display.print("[ ");
  display.printf("%4.1f", tensao_painel);
  display.setCursor(0, 60);
  display.print("] ");
  display.printf("%4.1f", tensao_bateria);
  display.setCursor(100, 32);
  Serial.print("Bomba: ");
  Serial.println(bomba);
  if(bomba) { //se a bomba estiver ligada
    display.setTextColor(BLACK, WHITE);
    display.print("B");
  }
  else display.print(" ");
  display.display();
}

void loop()
{
  delay(250);  
  leituras();
  publica_blink();
  publica_thingspeak();
  digitalWrite(led, LOW);
  mostra_display();

  //while(flag_toque==1) {   //loop se tocar no pino touch 13 - calibração
  //  leituras();
  //  mostra_display();
    //publica_blink();
  //  delay(300);
  //}

  if (bomba == 0)
  {    
    Serial.println("Bomba não foi ligada. Indo dormir");
    esp_deep_sleep_start(); //se a bomba nao foi ligada, pode ir domir
  }

  while (bomba_desl==0)   //loop se estiver com a bomba ligada
  {
    leituras();
    publica_blink();
    //Blynk.syncAll();
    Atualiza_data();
    if (BLYNK_F("Time sync: OK")) flag_atualiza_data = !flag_atualiza_data;
    else flag_atualiza_data = 0;
    mostra_display();
    delay(300);
  }
  publica_blink();
  publica_thingspeak();
  Serial.print("Bomba desligou e foi dormir");
  esp_deep_sleep_start(); //se a bomba desligou pode ir domir
  // int state = digitalRead(LED_BUILTIN);
  // digitalWrite(LED_BUILTIN, !state);
}