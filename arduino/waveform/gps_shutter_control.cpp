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
const int PIN_NEG = 3;                                // Match with hardware connection, odd negative pulses
const int PIN_POS = 4;                                // Match with hardware connection, even positive pulses
const byte NEG_MASK = (1 << 3);                       // Precalculate for fast interrupt handling
const byte POS_MASK = (1 << 4);                       // Precalculate for fast interrupt handling
const byte ZERO_MASK = 255 & ~NEG_MASK & ~POS_MASK;   // Precalcualte for fast interrupt handling
const float CPU_FREQ = 16000000.;                     // From Arduino specs
float calibratedFreq = CPU_FREQ;                      // Optionally set to a logged value from a previous session
const int N_HALF_WAVE = 32;                           // Inverse of BLOCK_TICKS;
const int PRESCALER = 64;                             // Prescaler value to be set for TIMER1
const int TICK_MICROS = 4;                            // TIMER1 resolution with prescaler at 64
const int N_STABLE = 10;                              // Number of successive GPS pulses required for stability
const int N_CALIBRATE = 20;                           // Number of successive GPS pulses required for calibration (< 4200)
const unsigned int TIMER_SAFETY = 2;                  // For being sure the duration of the pulse train < 1.000000 second

volatile bool gpsHit = false;                         // Set by the gpsIn interrupt only and cleared after processing
volatile unsigned long lastGpsMicros = 0;             // Set by the gpsIn interrupt only and ignored before gpsHit = true
unsigned long iGpsPulse = 0;                          // Number of successive GPS pulses counted
unsigned long prevGpsMicros = 4290000000;             // For incrementing iGpsPulse
unsigned long gpsStartMicros = 0;                     // Start time of a sequence of successive GPS pulses
volatile unsigned long iHalfWave = 0;                 // Phase of shutter waveform in terms of block half waves
volatile unsigned long iIsr = 0;                      // For debugging
float lastTaskWarningMillis;                          // Used to check if run_shutter_control is called in time
int halfWaveValues[36];                               // For debugging

/*
 * In the main loop lastGpsMicros is used:
 *  - to derive the GPS lock state
 *  - to calibrate the CPU clock
 * The gpsHit variable is used to trigger the control functions in the main loop.
 */
void gpsIn()
{
  cli();                                  // disable interrupts for making the value changes below in an atomic way for consistency
  lastGpsMicros = micros();               // the micros() value can be reliably read on entry of an ISR
  gpsHit = true;
  iIsr = 0;
  sei();
}

ISR(TIMER1_COMPA_vect)
{
  // Use the PORTD register to have pins PIN_NEG and PIN_POS switch simultaneously
  // Some added timing jitter between the start of the ISR and the new setting of PORTD
  // cannot be avoided, because the conditional branches below follow a different sequence
  // of instructions. However, this is at the microsecond level.
  // ihalfWave == 0: blanking period
  // iHalfWave == 1, 3, 5, ..., 31 odd, negative pulses on PIN3
  // iHalfWave == 2, 4, 6, ..., 30 even, positive pulses on PIN4
  byte pinVals;
  iHalfWave++;
  if ( (iHalfWave % 2 == 1) || (iGpsPulse && (iHalfWave % N_HALF_WAVE == 0)) ) {
    pinVals = (PORTD & ZERO_MASK);
  } else if (iHalfWave % 4 == 2) {
    pinVals = (PORTD & ZERO_MASK) | NEG_MASK;
  } else {
    pinVals = (PORTD & ZERO_MASK) | POS_MASK;
  }
  PORTD = pinVals;
  iIsr++;
  halfWaveValues[iIsr % 36] = iHalfWave;  // iIsr and iHalfWave should rotate synchronously, but iIsr may be equal to 32 sometimes
}

void setup_shutter_control()
{
  // PIN I/O configs
  attachInterrupt(digitalPinToInterrupt(PIN_GPS), gpsIn, RISING);
  pinMode(PIN_NEG, OUTPUT);
  pinMode(PIN_POS, OUTPUT);

  // TIMER1 configs for creating a continuous sequence of ISR interrupts
  // TIMER1 is available if the Arduino servo library is not required
  // TIMER0 is occupied by Arduino core for the millis()/micros() functions
  TCCR1A = 0x00;                                                    // reset TIMER1 control register
  TCCR1B = (1<<CS10) | (1<<CS11);                                   // set the prescalar at 64 (OCR1A ticks of 4 microsecond)
  TCCR1B |= (1<<WGM12);                                             // set CTC mode (Clear Timer on Compare)
  TIMSK1 |= (1<<OCIE1A);                                            // enable TIMER1 interrupts
  OCR1A = calibratedFreq / PRESCALER / N_HALF_WAVE - TIMER_SAFETY;  // initial TIMER1 compare value for 16 Hz block half wave
  TCNT1 = 0;                                                        // TIMER1 counter start value

  lastTaskWarningMillis = millis() - 3600000;  // No warning during the past hour
  Serial.println("Configuration of LCD shutter completed\nStabilizing...");
}

void run_shutter_control()
{
  char s[90];
//  unsigned long entryMicros = micros();  // Temporary during development

  if (!gpsHit) {
    return;
  }
  long phaseDiff = lastGpsMicros - prevGpsMicros - 1000000;  // Uncalibrated measurement
  if (abs(phaseDiff) < 10000) {                              // 2 x 0.5% tolerance of Arduino ceramic resonator
    iGpsPulse ++;                                            // Overflow takes 136 years
  } else {
    //ToDo: somehow this section can be entered two times in one second!
    //11:51:23.005 -> Unexpected GPS pulse arrival. Deviation: -996604 microsecon
    //11:51:23.052 -> Unexpected GPS pulse arrival. Deviation: -999816 microsecon
    //11:51:23.989 -> Unexpected GPS pulse arrival. Deviation: -37864 microsecond
    //11:51:24.035 -> Unexpected GPS pulse arrival. Deviation: -987736 microsecon
    //11:51:24.967 -> Unexpected GPS pulse arrival. Deviation: -37856 microsecond
    //11:51:25.015 -> Unexpected GPS pulse arrival. Deviation: -987744 microsecon
    //11:51:25.993 -> Unexpected GPS pulse arrival. Deviation: -47440 microsecond
    iGpsPulse = 0;
    gpsStartMicros = lastGpsMicros;
    if (millis() > 2000) {                                   // phaseDiff during 2s of startup is expected
      snprintf(s, 60, "Unexpected GPS pulse arrival. Deviation: %ld microseconds", phaseDiff);
      Serial.println(s);
    }
  }
  if (iGpsPulse > N_STABLE) {
    // Beware of concurrency issues; do not touch TIMER1 close to an ISR, so delay for a while
    // OCR1A is calculated such that this should not happen
    unsigned int delayTicks = OCR1A - TCNT1;
    if (delayTicks < 128) {
      Serial.println("Avoidance triggered");
      delayMicroseconds((delayTicks + 8) * TICK_MICROS);
    }

    // Phase lock mechanisms 1 for start of pulse train each second (see explanation at top of file)
    // Phase = iHalfWave * OCR1A + TCNT1
    unsigned long oldHalfWave = iHalfWave;                              // for printing phaseDiff below
    unsigned int oldTCNT1 = TCNT1;                                      // for printing phaseDiff below
    unsigned long observedDiff = micros() - lastGpsMicros;              // small positive value, depending on other tasks in loop()
    unsigned long observedTicks = observedDiff / TICK_MICROS;
    unsigned int newTCNT1 = observedTicks % OCR1A;
    unsigned long newHalfWave = (observedTicks / OCR1A) % N_HALF_WAVE;
    TCNT1 = newTCNT1;
    iHalfWave = newHalfWave;

    // Log experienced phase difference to serial monitor
    oldHalfWave = oldHalfWave % N_HALF_WAVE;
    snprintf(s, 90, "LCD phase: %lu %lu %lu %u", iIsr, observedTicks, oldHalfWave, oldTCNT1);
    Serial.println(s);

    if ( (iGpsPulse % N_CALIBRATE) == 0) {
      // Phase lock mechanisms 2 for entire pulse train (see explanation at top of file)
      // Set OCR1A to the ideal value, calculated from the calibrated CPU clock
      // Apply *** down *** rounding for the OCR1A calculation so that the block frequency is slightly
      // too high rather than slightly too low and the syncing occurs during the blanking period
      unsigned long calibrationMicros = lastGpsMicros - gpsStartMicros;
      calibratedFreq = CPU_FREQ * calibrationMicros / iGpsPulse / 1000000.;
      OCR1A = calibratedFreq / PRESCALER / N_HALF_WAVE - TIMER_SAFETY;
      snprintf(s, 60, "Micros: %lu %lu", iGpsPulse, calibrationMicros);
      Serial.println(s);
      snprintf(s, 60, "CPU: %ld", (long)calibratedFreq);
      Serial.println(s);
      snprintf(s, 60, "Block: %u ticks", OCR1A);
      Serial.println(s);

    // Phase lock mechanisms 3 (see explanation at top of file)
    // Will not do; change to Nucleo-32 STM32G431 with crystal clock and 32-bit hardware timers
    }
  }

//  // For debugging: print iHalfwave with respect to GPS pulse
//  snprintf(s, 90, "hws %u %u %u ... %u %u %u",
//    halfWaveValues[0], halfWaveValues[1], halfWaveValues[2],
//    halfWaveValues[29], halfWaveValues[30], halfWaveValues[31]);
//  Serial.println(s);

  // Check behaviour of other tasks in the waveform.ino loop() function
  if (micros() - lastGpsMicros > 50000 && millis() - lastTaskWarningMillis > 3600000) {
    lastTaskWarningMillis = millis();
    Serial.println("WARN - tasks other than LCD shutter control take longer than 50 ms!");
  }

  // Prepare for next GPS 1 Hz pulse
  prevGpsMicros = lastGpsMicros;
  gpsHit = false;

//  // For debugging
//  snprintf(s, 90, "Loop duration: %lu", micros() - entryMicros);
//  Serial.println(s);
}
