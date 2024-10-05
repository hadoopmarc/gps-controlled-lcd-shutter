/* Script with experiments to possibly have the LCD shutter open faster (closing is fast by default):
   - block waveform that ends with a short pulse of inverted voltage
   - block waveform that alternates fast between positive ande negative continously
*/
const int pinNeg = 2;
const int pinPos = 4;
const byte NEG_MASK = B00000100;
const byte POS_MASK = B00010000;
const byte ZERO_MASK = B11101011;


int switchPins(int value)
{
  // Use the PORTD register to have pins 2 and 4 switch simultaneously
  // Some time jitter is possible because the branches below follow a different sequence of instructions
//  byte pinVals;
  if (value == 0) {
      digitalWrite(pinNeg, LOW);
      digitalWrite(pinPos, LOW);
      // pinVals = (PORTD & ZERO_MASK);
  } else if (value == -1) {
      digitalWrite(pinNeg, HIGH);
      digitalWrite(pinPos, LOW);
      // pinVals = (PORTD & ZERO_MASK) | NEG_MASK;
  } else {
      digitalWrite(pinNeg, LOW);
      digitalWrite(pinPos, HIGH);
      //pinVals = (PORTD & ZERO_MASK) | POS_MASK;
  }
//  PORTD = pinVals;
  return 0; //pinVals;
}

void setup()
{
  pinMode(pinNeg, OUTPUT);
  pinMode(pinPos, OUTPUT);
  Serial.begin(2000000);
}

void shortInverted()
{
  const int HALF_WAVE = 32;    // milliseconds
  const int INV_PULSE = 5000;  // microseconds
  switchPins(1);
  delay(HALF_WAVE);
// Commenting this out shows that the short pulse has no influence
// because successive pulses have the same shape
//  switchPins(-1);
  delayMicroseconds(INV_PULSE);
  switchPins(0);
  delay(HALF_WAVE);
  switchPins(-1);
  delay(HALF_WAVE);
  switchPins(1);
  delayMicroseconds(INV_PULSE);
  switchPins(0);
  delay(HALF_WAVE);  
}

void fastPositiveNegative()
{
  // The sequences of 2 ms alternating positive and negative pulses neither
  // have any influence on the opacity waveform of the LCD-shutter, compared
  // to a simple 32 ms halfwave block waveform.
  const int HALF_WAVE = 32;  // milliseconds
  const int PULSE = 2;       // milliseconds
  for (int i=0; i<HALF_WAVE/(2*PULSE); i++) {
    switchPins(1);
    delay(PULSE);
    switchPins(-1);
    delay(PULSE);
  }
  switchPins(0);
  delay(HALF_WAVE);
  for (int i=0; i<HALF_WAVE/(2*PULSE); i++) {
    switchPins(1);
    delay(PULSE);
    switchPins(-1);
    delay(PULSE);
  }
  switchPins(0);
  delay(HALF_WAVE);
}

void loop()
{
  while(true)
  {
    // Uncomment one of the modes below
    //pshortInverted();
    fastPositiveNegative();
  }
}
