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
#include <Wire.h> // Wire header file for I2C and 2 wire

unsigned long myChannelNumber = 38484;
const char * myWriteAPIKey = "UW9T0WNPQPVY7QPB";
WiFiClient  client;

char blynk_token[34] = "591b947a24354dd085ef3ae6d7ffa399";

const int led = 19;
const int pin_adc_1 = 35; //GPIO usado para captura analógica
const int pin_adc_2 = 32; //GPIO usado para captura analógica
uint16_t n=0;
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

//Configurações TMP75
float lastTemp;
float lastHum;
int metric = true;

int8_t TMP75_Address = 0x48; //
int8_t configReg = 0x01;     // Address of Configuration Register
int8_t bitConv = 0x60;       //01100000;  // Set to 12 bit conversion
int8_t rdWr = 0x01;          // Set to read write
int8_t rdOnly = 0x00;        // Set to Read
int decPlaces = 1;
int numOfBytes = 2;

void setup() {
  pinMode(led, OUTPUT);
  digitalWrite(led,HIGH);
  bootCount++;
  Serial.begin(115200);

  //read configuration from FS json
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

  //Setup TMP75
  Wire.begin();                            //  Wire.begin(SDA,SCL,BUS_SPEED);     Join the I2C bus as a master
  Wire.beginTransmission(TMP75_Address); // Address the TMP75 sensor
  Wire.write(configReg);                 // Address the Configuration register
  Wire.write(bitConv);                   // Set the temperature resolution
  Wire.endTransmission();                // Stop transmitting
  Wire.beginTransmission(TMP75_Address); // Address the TMP75 sensor
  Wire.write(rdOnly);                    // Address the Temperature register
  Wire.endTransmission();                // Stop transmitting
}

// Begin the reading the TMP75 Sensor
float readTemp()
{
  // Now take a Temerature Reading
  Wire.requestFrom(TMP75_Address, numOfBytes); // Address the TMP75 and set number of bytes to receive
  int8_t MostSigByte, LeastSigByte;
  while (Wire.available()) { // Checkf for data from slave
    int8_t MostSigByte = Wire.read();  // Read the first byte this is the MSB
    int8_t LeastSigByte = Wire.read(); // Now Read the second byte this is the LSB
    Serial.println("Fez a medida da temperatura");
  }

  // Being a 12 bit integer use 2's compliment for negative temperature values
  int TempSum = (((MostSigByte << 8) | LeastSigByte) >> 4);
  // From Datasheet the TMP75 has a quantisation value of 0.0625 degreesC per bit
  float temp = (TempSum * 0.0625);
  //Serial.println(MostSigByte, BIN);   // Uncomment for debug of binary data from Sensor
  //Serial.println(LeastSigByte, BIN);  // Uncomment for debug  of Binary data from Sensor
  return temp; // Return the temperature value  
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
  float temperature = readTemp();
  Serial.print("TMP75: ");
  Serial.println(temperature);

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
