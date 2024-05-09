/* Script that experiments with:
   - timing jitter from a free loop
   - timing jitter from an interrupt-based loop
   - writing two digital pins simultaneously

   It might be useful for certain tests as well
*/
const unsigned long blockMicros = 2000000;
const int zeroVal = 127;
const int pinNeg = 3;
const int pinPos = 4;
const byte NEG_MASK = B00000100;
const byte POS_MASK = B00010000;
const byte ZERO_MASK = B11101011;

unsigned long startMicros;
byte previousPinVals;


// Register settings, see: https://exploreembedded.com/wiki/AVR_Timer_programming
// Interrupt setting, see: https://circuitdigest.com/microcontroller-projects/arduino-timer-tutorial
// Arduino core AVR uses TIMER0 for millis()/micros()
// TIMER1 can be used if the servo library is not needed
// TIMER1 is configured to CTC mode (Clear Timer on Compare)
ISR(TIMER1_COMPA_vect)                   
{
  char buf[50];
  unsigned long relMicros = micros() - startMicros;
  ltoa(relMicros, buf, 10);
  Serial.write(buf);
  Serial.println(" TIMER1 interrupt occured!");
}

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

// Note: Arduino UNO uses a cheap ceramic oscillator with +/- 0.5% frequency tolerance:
//    https://www.arrow.com/en/research-and-events/articles/oscillators-and-arduino-configurations-and-settings
void setup()
{
  pinMode(pinNeg, OUTPUT);
  pinMode(pinPos, OUTPUT);
  Serial.begin(2000000);
  startMicros = micros();
  previousPinVals = PORTD;

  // Testing TIMER1 interrupts
  TCCR1A = 0x00;
  TCCR1B = (1<<CS12);                // set the prescalar at 256
  TCCR1B |= (1 << WGM12);            // CTC mode (Clear Timer on Compare)
  OCR1A = 62499;                     // compare value for 1000ms delay
  TCNT1 = 0;                         // start value
  TIMSK1 |= (1<<OCIE1A);             // enable TIMER1 interrupts
}

void loop()
{
  // Without timers the delay visible on the serial console varies from 0 to 40 microseconds
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
