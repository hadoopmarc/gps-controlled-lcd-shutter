 int ledPin = 13;
 
 void setup() {
  Serial.begin(9600);
  Serial.println("Hello meteors!");
  pinMode(ledPin, OUTPUT);
 }
 
 void loop() {
  digitalWrite(ledPin, !digitalRead(ledPin));
  delay(500);
 }
 
