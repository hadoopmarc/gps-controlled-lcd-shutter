/*
Script for generating digital signals as input for a driver of an LCD-shutter.
The signals have a specific pattern useful for meteor photography and are synced
to the 1 Hz GPS TIMEPULSE signal, also called PPS signal. For this, the script
applies three mechanisms:
1. Shortly after each GPS pulse, so during the blanking period of a 1 second cycle, a
   new train of 15 pulses is started that initially is synced very closely to the GPS
   signal.
2. The CPU frequency is measured during a relatively long period (> 60 seconds) with
   the GPS pulses as an accurate reference. After that, the timing of the train of 15
   pulses is done using this calibrated CPU frequency. This significantly improves
   the syncing of the pulse train at the end of a cycle. Note that the Arduino clock
   has a ceramic resonator with a frequency of 16 MHz +/- 0.5% (so, not very accurate).
3. Timing of the train of 15 pulses is done with interrupts triggered by a hardware
   timer of the Arduino CPU, which has a granularity of 4 microseconds. After
   switching the outputs for 32 times during a 1 second cycle, this results in a
   maximum phase difference with the GPS signal of 128 microseconds (after applying
   mechanism 2). Mechanism 3 further reduces the phase difference by varying the
   individual durations in a train of 15 pulses such that the sum of the residuals is
   minimal.

Future versions of the script might write the clibrated CPU frequency to the EEPROM
of the Arduino board automatically, so that full accuracy is obtained from the start
of each session.

Without a GPS signal the script operates in a free running mode generating 16 pulses
per second using the edited constant CPU_FREQ as a reference.

Documentation used as input for this script:
 - register settings: https://exploreembedded.com/wiki/AVR_Timer_programming
 - interrupt settings: https://circuitdigest.com/microcontroller-projects/arduino-timer-tutorial
 - CPU clock accuracy: https://www.arrow.com/en/research-and-events/articles/oscillators-and-arduino-configurations-and-settings

The code below uses micros() for time keeping based on the CPU clock. The Arduino library uses a 32-bit
counter for the micros() function, which overflows after about 70 minutes. Therefore, only time differences
between successive micros() calls are used, which are always correct irrespective of any overflow that
occurred between the two calls.
*/
#include <arduino.h>

const int PIN_GPS = 2;                                // Match with hardware connection
const int PIN_NEG = 3;                                // Match with hardware connection
const int PIN_POS = 4;                                // Match with hardware connection
const byte NEG_MASK = (1 << 3);                       // Precalcualte for fast interrupt handling
const byte POS_MASK = (1 << 4);                       // Precalcualte for fast interrupt handling
const byte ZERO_MASK = 255 & ~NEG_MASK & ~POS_MASK;   // Precalcualte for fast interrupt handling
float CPU_FREQ = 16000000.;                           // An estimate or a measured value from a previous session
const unsigned long BLOCK_TICKS = 7812;               // Pulse width of a 16 Hz block half wave (in TICK_MICROS)
const int N_HALF_WAVE = 32;                           // Inverse of BLOCK_TICKS;
const int TICK_MICROS = 4;                            // TIMER1 resolution with prescaler at 64
const int N_STABLE = 10;                              // Number of successive GPS pulses required for stability
const int N_CALIBRATE = 20;                           // Number of successive GPS pulses required for calibration (< 4200)

volatile bool gpsHit = false;                         // Set by the gpsIn interrupt only and cleared after processing
volatile unsigned long lastGpsMicros = 0;             // Set by the gpsIn interrupt only and ignored before gpsHit = true
unsigned long iGpsPulse = 0;                          // Number of successive GPS pulses counted
unsigned long prevGpsMicros = 4290000000;             // For incrementing iGpsPulse
unsigned long gpsStartMicros = 0;                     // Start time of a sequence of successive GPS pulses

volatile unsigned long iHalfWave = 0;                 // Phase of shutter waveform in terms of block half waves
float lastTaskWarningMillis;                          // Used to check if run_shutter_control is called in time

/*
 * In the main control loop lastGpsMicros is used:
 *  - to derive the GPS lock state
 *  - to calibrate the CPU clock
 * The gpsHit variable is used to trigger the control functions in the main loop.
 */
void gpsIn()
{
  // micros() updates are postponed during interrupt handling, but reading the value on entry is valid
  lastGpsMicros = micros();
  gpsHit = true;
}

ISR(TIMER1_COMPA_vect)
{
  // Use the PORTD register to have pins PIN_NEG and PIN_POS switch simultaneously
  // Some added timing jitter between the start of the ISR and the new setting of PORTD
  // cannot be avoided, because the conditional branches below follow a different sequence
  // of instructions. However, this is at the microsecond level.
  byte pinVals;
  if ( (iHalfWave % 2 == 1) || (iGpsPulse && (iHalfWave % N_HALF_WAVE == 0)) ) {
    pinVals = (PORTD & ZERO_MASK);
  } else if (iHalfWave % 4 == 2) {
    pinVals = (PORTD & ZERO_MASK) | NEG_MASK;
  } else {
    pinVals = (PORTD & ZERO_MASK) | POS_MASK;
  }
  PORTD = pinVals;
  iHalfWave++;
}

void setup_shutter_control()
{
  // PIN I/O configs
  attachInterrupt(digitalPinToInterrupt(PIN_GPS), gpsIn, RISING);
  pinMode(PIN_NEG, OUTPUT);
  pinMode(PIN_POS, OUTPUT);

  // TIMER1 configs for creating a continuous sequence of ISR interrupts
  // TIMER0 is occupied by Arduino core for the millis()/micros() functions
  // TIMER1 can be used if the Arduino servo library is not required
  TCCR1A = 0x00;                       // reset TIMER1 control register
  TCCR1B = (1<<CS10) | (1<<CS11);      // set the prescalar at 64 (OCR1A ticks of 4 microsecond)
  TCCR1B |= (1<<WGM12);                // set CTC mode (Clear Timer on Compare)
  TIMSK1 |= (1<<OCIE1A);               // enable TIMER1 interrupts
  OCR1A = BLOCK_TICKS;                 // initial compare value for 16 Hz block half wave
  TCNT1 = 0;                           // TIMER1 counter start value

  lastTaskWarningMillis = millis() - 3600000;  // No warning during the past hour
  Serial.println("Configuration of LCD shutter completed\nStabilizing...");
}

void run_shutter_control()
{
  char s[90];
  unsigned long entryMicros = micros();  // Temporary during development

  if (!gpsHit) {
    return;
  }
  long phaseDiff = micros() - prevGpsMicros - 1000000;
  if (abs(phaseDiff) < 5000) {   // 0.5% tolerance of Arduino ceramic resonator
    iGpsPulse ++;   // Overflow takes 136 years
  } else {
    iGpsPulse = 0;
    gpsStartMicros = lastGpsMicros;
    if (millis() > 2000) {  // phaseDiff during startup is expected
      snprintf(s, 60, "GPS pulse out of order. Deviation: %ld microseconds", phaseDiff);
      Serial.println(s);
    }
  }
  if (iGpsPulse > N_STABLE) {
    // Phase lock mechanisms 1 (see top of file). Phase = iHalfWave * OCR1A + TCNT1
    phaseDiff = micros() - lastGpsMicros -
      ((iHalfWave % N_HALF_WAVE) * (unsigned int)OCR1A + TCNT1) * TICK_MICROS;  // For printing only
    unsigned long phaseTicks = (micros() - lastGpsMicros) / TICK_MICROS;
    if (OCR1A - TCNT1 > 1) {  // Beware of concurrency issues; leave alone close to an ISR call
      TCNT1 = phaseTicks % OCR1A;
      iHalfWave = (phaseTicks / OCR1A) % N_HALF_WAVE;
    }
    snprintf(s, 90, "LCD shutter out of phase by %ld microsecond %lu %lu", phaseDiff, micros(), lastGpsMicros);
    Serial.println(s);

    if ( (iGpsPulse % N_CALIBRATE) == 0) {
      // Phase lock mechanisms 2 (see top of file). Set OCR1A to the ***down*** rounded ideal
      // value, calculated from the calibrated CPU clock
      float calibratedFreq = CPU_FREQ * (lastGpsMicros - gpsStartMicros) / iGpsPulse / 1000000.;
      OCR1A = 7818; //(unsigned int)(BLOCK_TICKS * calibratedFreq / CPU_FREQ);
      snprintf(s, 60, "Calibrated CPU clock frequency (kHz): %ldf", (long)calibratedFreq);
      Serial.println(s);
      snprintf(s, 60, "New value for OCR1A (in 4 microsecond ticks): %u", OCR1A);
      Serial.println(s);

      // Phase lock mechanisms 3 (see top of file).
    }
  }

  // Check behaviour of other tasks in the waveform.ino loop() function
  if (micros() - lastGpsMicros > 50000 && millis() - lastTaskWarningMillis > 3600000) {
    lastTaskWarningMillis = millis();
    Serial.println("WARN - tasks other than LCD shutter control take longer than 50 ms!");
  }

  // Prepare for next GPS 1 Hz pulse
  prevGpsMicros = lastGpsMicros;
  gpsHit = false;
  snprintf(s, 90, "Loop duration: %lu", micros() - entryMicros);
  Serial.println(s);
}

//Problem: with a calculated OCR1A > 7817 phaseDiff experiences a sign problem
//21:36:03.367 -> Loop duration: 3968
//21:36:04.306 -> LCD shutter out of phase by -1000664 microsecond 38145180 38145068
//21:36:04.353 -> Loop duration: 3964
//21:36:05.293 -> LCD shutter out of phase by -1000668 microsecond 39146008 39145900
//21:36:05.386 -> Loop duration: 3964
//21:36:06.278 -> LCD shutter out of phase by -1000668 microsecond 40146840 40146732
//21:36:06.372 -> Calibrated CPU clock frequency (kHz): 16013299f
//21:36:06.419 -> New value for OCR1A (in 4 microsecond ticks): 7818
//21:36:06.465 -> Loop duration: 108984
//21:36:07.312 -> LCD shutter out of phase by -1000668 microsecond 41147672 41147564
//21:36:07.359 -> Loop duration: 3972
//21:36:08.297 -> LCD shutter out of phase by -1000656 microsecond 42148504 42148392
//21:36:08.390 -> Loop duration: 3952
