#include <arduino.h>

void setup() {
  Serial.begin(115200);
}

void loop() {
  Serial.println(random());
  Serial.println("<camstart>");
  delay(1000);
  Serial.println(random());
  Serial.println("<camstop>");
  delay(1000);
}
