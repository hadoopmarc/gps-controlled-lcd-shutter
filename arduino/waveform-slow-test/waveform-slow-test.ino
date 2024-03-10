const unsigned long blockMicros = 2000000;
const int zeroVal = 127;
const int pinNum = 5;

unsigned long startMicros;
int previousPinVal;


int getAnalogValue(unsigned long diffMicros)
{
  long nHalfWaves = diffMicros / blockMicros;
  if (nHalfWaves % 2 == 1) {
      return 127;
  } else if (nHalfWaves % 4 == 2) {
      return 0;
  } else {
      return 255;
  }
}

void setup()
{
  pinMode(pinNum, OUTPUT);
  Serial.begin(2000000);
  startMicros = micros();
  previousPinVal = 50;
}

void loop()
{
  char buf[50];
  // Note: overflow does not interfere with small differences
  unsigned long relMicros = micros() - startMicros;
  int pinVal = getAnalogValue(relMicros);
  analogWrite(pinNum, pinVal);
  if (pinVal != previousPinVal) {
    ltoa(relMicros, buf, 10);
    Serial.write("New pin output at ");
    Serial.write(buf);
    Serial.write(": ");
    Serial.println(pinVal);
  }
  previousPinVal = pinVal;
}
