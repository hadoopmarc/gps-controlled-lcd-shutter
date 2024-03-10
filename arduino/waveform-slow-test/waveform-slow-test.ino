const unsigned long blockMicros = 2000000;
const int zeroVal = 127;
const int pinNeg = 2;
const int pinPos = 5;
const byte NEG_MASK = B00000100;
const byte POS_MASK = B00010000;
const byte ZERO_MASK = B11101011;

int hoi;
unsigned long startMicros;
byte previousPinVals;


int switchPins(unsigned long diffMicros)
{
  long nHalfWaves = diffMicros / blockMicros;
  // Use the PORTD register to have pins 2 and 4 switch simultaneously
  // Some jitter is possible because the branches below follow a different sequence of instructions
  byte pinVals;
  if (nHalfWaves % 2 == 1) {
      pinVals = (PORTD & ZERO_MASK);
  } else if (nHalfWaves % 4 == 2) {
      pinVals = (PORTD & ZERO_MASK) | NEG_MASK;
  } else {
      pinVals = (PORTD & ZERO_MASK) | POS_MASK;
  }
  PORTD = pinVals;
  return pinVals;
}

void setup()
{
  pinMode(pinNeg, OUTPUT);
  pinMode(pinPos, OUTPUT);
  Serial.begin(2000000);
  startMicros = micros();
  previousPinVals = PORTD;
}

void loop()
{
  char buf[50];
  // Note: overflow does not interfere with small differences
  unsigned long relMicros = micros() - startMicros;
  byte pinVals = switchPins(relMicros);
  if (pinVals != previousPinVals) {
    ltoa(relMicros, buf, 10);
    Serial.write("New pin output at ");
    Serial.write(buf);
    Serial.write(": ");
    Serial.println(pinVals);
  }
  previousPinVals = pinVals;
}
