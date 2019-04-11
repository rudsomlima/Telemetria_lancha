#include <Arduino.h>


const int led = 19;
const int pin_adc_1 = 35; //GPIO usado para captura analógica
const int pin_adc_2 = 32; //GPIO usado para captura analógica
uint16_t n=0;

void setup() {
  Serial.begin(115200);
  adcAttachPin(pin_adc_1);
  adcAttachPin(pin_adc_2);
  // analogSetClockDiv(255); // 1338mS
	// adcStart(pin_adc);
	analogReadResolution(10); // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
	// analogSetAttenuation(ADC_6db); // Default is 11db which is very noisy. Recommended to use 2.5 or 6.
  // pinMode(pin_adc, INPUT); //Pino utilizado para captura analógica
  pinMode(led, OUTPUT);
}

void loop() {
  int bat_int = analogRead(pin_adc_1);
  int bat_12v = analogRead(pin_adc_2);
  float tensao_1 = bat_int*3.3/1024;
  float tensao_2 = bat_12v*3.3/1024;
  // // print out the value you read:
  Serial.print(n);
  Serial.print(" - bat_int: ");
  Serial.println(tensao_1);
  Serial.print(n);
  Serial.print(" - bat_12v: ");
  Serial.println(tensao_2);
  n++;
  digitalWrite(led,LOW);
  delay(200);
  digitalWrite(led,HIGH);
  delay(200);

  // int state = digitalRead(LED_BUILTIN);
  // digitalWrite(LED_BUILTIN, !state);
}
