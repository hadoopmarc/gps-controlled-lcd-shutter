/* 
 * Simple rain monitor using a B+B Thermo-Technik 2020 version rain sensor
 * Time library: https://github.com/PaulStoffregen/Time
 */
#include <TimeLib.h>

const int rainPin = 6;

void setup() {
  Serial.begin(9600);
  setTime(20,55,0,21,10,2024);  // Edit before upload/start

  // configure pin D6 as an input and enable the internal pull-up resistor
  pinMode(rainPin, INPUT_PULLUP);  // REL NO (green); REL CO to ground (brown)
}

void loop() {
  int S = 90;
  char s[S];

  time_t t = now();
  snprintf(s, S, "%4d-%02d-%02d %02d:%02d:%02d ",
    year(t), month(t), day(t), hour(t), minute(t), second(t));
  Serial.print(s);
  // Input LOW means:  sensor relay closed -> wet conditions
  // Input HIGH means: sensor relay open -> dry conditions
  int sensorVal = digitalRead(rainPin);

  if (sensorVal == HIGH) {
    Serial.println("Dry");
  } else {
    Serial.println("Wet");
  }

  delay(10000);
}