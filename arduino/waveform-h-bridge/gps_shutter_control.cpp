/*
Script for generating digital signals as input for an H-bridge driver of an LCD-shutter.
The signals have a specific pattern useful for meteor photography and are synced
to the 1 Hz GPS TIMEPULSE signal, also called PPS signal. For this, the script
applies three mechanisms:
1. Shortly after each GPS pulse, so during the blanking period of a 1 second cycle, a
   new train of 16 pulses is started that initially is synced very closely to the GPS
   signal.
2. The MCU frequency is measured during a relatively long period (>= 60 seconds) with
   the GPS pulses as an accurate reference. After that, the timing of the train of 15
   pulses is done using this calibrated MCU frequency. This significantly improves
   the syncing of the pulse train at the end of a cycle. Note that the Arduino clock
   has a ceramic resonator with a frequency of 16 MHz +/- 0.5% (so, not very accurate).
3. Timing of the train of 15 pulses is done with interrupts triggered by a hardware
   timer of the Arduino MCU, which has a granularity of 4 microseconds. After
   switching the outputs for 32 times during a 1 second cycle, this results in a
   maximum phase difference with the GPS signal of 128 microseconds (after applying
   mechanism 2). Mechanism 3 could further reduce the phase difference by varying the
   individual durations in a train of 16 pulses such that the sum of the residuals is
   minimal.

Without a GPS signal the script operates in a free running mode generating 16 pulses
per second using the calibratedFreq variable as a reference (boot value can be edited).

Documentation used as input for this script:
 - register settings:  https://exploreembedded.com/wiki/AVR_Timer_programming
 - interrupt settings: https://circuitdigest.com/microcontroller-projects/arduino-timer-tutorial
 - MCU clock accuracy: https://www.arrow.com/en/research-and-events/articles/oscillators-and-arduino-configurations-and-settings

The code below uses micros() for time keeping based on the MCU clock. The Arduino library uses a 32-bit
counter for the micros() function, which overflows after about 70 minutes. Therefore, only time differences
between successive micros() calls are used, which are always correct irrespective of any overflow that
occurred between the two calls.

The code below implements a state machine with the states described below (stored as iGpsPulse).
|-> INIT - entered at the start or after receiving an out-of-phase GPS pulse
|    |
|    |
|    |-> ZERO - entered when receiving the zeroth GPS pulse while being in the INIT state
|        |
|        |
|--------|-> STABLE - entered when receiving GPS pulse number N_STABLE while being in the FIRST state
|            |
|            |
|            |-> CALIBRATED - entered when receiving GPS pulse number N_CALIBRATE while being in the STABLE state
|----------------|

In the STABLE and CALIBRATED states the pulse trains are locked to the GPS signal and the following events occur
each second (see the signal diagram in the README of the repo root):
1. just before the GPS pulse arrives, a new pulse train has started with iHalfWave = 32. Outside the locked state
   this would be a positive electrical voltage using POS_MASK, but in the locked state this triggers a specific
   condition resulting in an electrical shortcut state using HIGH_MASK.
2. the GPS pulse arrives and sets gpsHit to true
3. the shutter_control loop detects the gspHit state and starts correcting small phase differences by manipulating
   the value of TCNT1. Both leaving the control logic, iHalfWave should be zero.
*/
#define DEBUG_LOG
#undef DEBUG_LOG                                      // Outcomment to enable debug logging

#include <arduino.h>
#include "gps_shutter_control.h"

// Constant values
const int PIN_GPS = 2;                                // Match with hardware connection
const int PIN_NEG = 3;                                // Match with hardware connection, odd negative pulses
const int PIN_POS = 4;                                // Match with hardware connection, even positive pulses
const byte NEG_MASK = (1 << 3);                       // Precalculate for fast interrupt handling
const byte POS_MASK = (1 << 4);                       // Precalculate for fast interrupt handling
const byte HIGH_MASK = NEG_MASK | POS_MASK;           // Additional mask for H-bridge driver
const byte ZERO_MASK = 255 & ~NEG_MASK & ~POS_MASK;   // Precalcualte for fast interrupt handling
const int MCU_MHZ = 16;                               // From Arduino specs
const int N_WAVE = 16;                                // The shutter frequency
const int N_HALF_WAVE = 32;                           // Twice the shutter frequency
const int PRESCALER = 64;                             // Prescaler value to be set for TIMER1
const int TICK_MICROS = 4;                            // TIMER1 resolution with prescaler at 64
const int N_INIT = -1;                                // iGpsPulse value for the INIT state (no GPS pulse received)
const int N_ZERO = 0;                                 // iGpsPulse value for the ZERO state (first GPS pulse interval started)
const int N_STABLE = 10;                              // iGpsPulse value for the STABLE state: starting second markers (short GPS calibration interval)
const int N_CALIBRATE = 60;                           // iGpsPulse value for the CALIBRATED state (< 4200): GPS-calibrated second markers
const int TIMER_SAFETY = 2;                           // For being sure the duration of the pulse train < 1.000000 second
                                                      // This is for 1 wave period, so impact = N * 16 * 4 = N * 64 microseconds

// Global variables modified in interrupt routines
volatile bool gpsHit = false;                         // Set by the gpsIn interrupt only and cleared after processing
volatile unsigned long lastGpsMicros;                 // Set by the gpsIn interrupt only and ignored before gpsHit = true
volatile unsigned long iHalfWave = 0;                 // Phase of shutter waveform in terms of block half waves
volatile unsigned long iIsr = 0;                      // For monitoring

// Global variables related to and depending on calibration
int shutPercentage;                                   // Value passed to setup_shutter_control()
int compensationTicks;                                // Code execution duration from time measurement to timer adjustment
unsigned long calibratedFreq = 1000000 * MCU_MHZ;     // Overwritten by initial calibration after 10 GPS pulse intervals
unsigned int waveTicks;                               // Number of ticks of 1 wave of 16 Hz (depends on auto-calibration)
unsigned int ocr1aShut;                               // Precalculated timer value based on shutPercentage and auto-calibration
unsigned int ocr1aOpen;                               //       ,,        ,,    ,,    ,,          ,,                  ,,

// Global variables related to the lock state
unsigned long iGpsPulse = -1;                         // Number of successive GPS pulse intervals counted for stabilization and calibration
unsigned long prevGpsMicros;                          // For checking timely arrival of current iGpsPulse
unsigned long gpsStartMicros;                         // Start time of a sequence of successive GPS pulses

// Global character buffer for sprintf() + Serial.println()
const int S = 90;
char s[S];

// Global variable related to debug logging
#ifdef DEBUG_LOG
const int NLOG = 100;                                 // About 3 seconds of hispeed logging
volatile int iLog = -960;                             // This logging starts after 30 seonds of operation
volatile unsigned long logIsr[NLOG];                  // Stores iIsr values
volatile unsigned long logHalf[NLOG];                 // Stores iHalfWave values
volatile unsigned long logMicros[NLOG];               // Stores micros()
#endif

/*
 * In the run_shutter_control() loop lastGpsMicros is used:
 *  - to derive the GPS lock state
 *  - to calibrate the MCU clock
 * The gpsHit variable is used to trigger the control logic in the run_shutter_control() loop.
 */
void gpsIn()
{
  cli();                                  // disable interrupts for making the value changes below in an atomic way
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
  // ihalfWave == 0 or 32: blanking period
  // iHalfWave == 1, 3, 5, ..., 31 odd, negative pulses on PIN3
  // iHalfWave == 2, 4, 6, ..., 30 even, positive pulses on PIN4
  //
  // The LCD-shutter needs the slow decay mode of the H-bridge to become transparent. In the slow decay mode the
  // terminals of the LCD-shutter are shortcut. The slow decay mode requires a logical high signal on both input
  // terminals of the TB6612 module. This is different compared to the script version for controlling an
  // opamp-based driver.

  iIsr++;                     // At the end of a pulse train, iIsr and iHalfwave get a value 32 here;
  iHalfWave++;                // Directly after a GPS pulse they are set to 0 by run_shutter_control()

  byte pinVals;
  if ((iHalfWave % 2 == 1) || (iGpsPulse >= N_STABLE && iHalfWave % N_HALF_WAVE == 0)) {
    pinVals = (PORTD & ZERO_MASK) | HIGH_MASK;
  } else if (iHalfWave % 4 == 2) {
    pinVals = (PORTD & ZERO_MASK) | NEG_MASK;
  } else {
    pinVals = (PORTD & ZERO_MASK) | POS_MASK;
  }
  PORTD = pinVals;

  // Alternate the "shut" and "open" timer compare values to realize the set shutPercentage
  // Not combined with setting pinVals to avoid unnecessary execution time delays
  if (iHalfWave % 2 == 1) {
    OCR1A = ocr1aOpen;        // Corresponds to HIGH_MASK -> shutter terminals shortcut
  } else {
    OCR1A = ocr1aShut;        // Corresponds to NEG_MASK and POS_MARK -> voltage on shutter
  }

  // Optional highspeed logging for debugging
  #ifdef DEBUG_LOG
  if (iLog >= 0 && iLog < NLOG) {
    logIsr[iLog] = iIsr;
    logHalf[iLog] = iHalfWave;
    logMicros[iLog] = micros();
  }
  iLog++;
  #endif
}

void calibrate(unsigned int nPulse, unsigned long calibrationMicros)
{
  // Phase lock mechanism 2 for entire pulse train (see explanation at top of the file)
  // Set OCR1A to the ideal value, calculated from the calibrated MCU clock
  // Apply *** down *** rounding for the OCR1A calculation so that the block frequency is slightly
  // too high rather than slightly too low and the syncing occurs during the blanking period
  calibratedFreq = calibrationMicros / nPulse * MCU_MHZ;
  waveTicks = calibratedFreq / PRESCALER / N_WAVE - TIMER_SAFETY;
  ocr1aShut = (long)waveTicks * shutPercentage / 100;
  ocr1aOpen = waveTicks - ocr1aShut;
  snprintf(s, S, "Micros: %lu", calibrationMicros);
  Serial.println(s);
  snprintf(s, S, "MCU: %lu", calibratedFreq);
  Serial.println(s);
  snprintf(s, S, "Wave: %u %u ticks", ocr1aShut, ocr1aOpen);
  Serial.println(s);

// Phase lock mechanisms 3 (see explanation at top of file)
// Will not do; change to Nucleo-32 STM32G431 with crystal clock and 32-bit hardware timers
}

void setup_shutter_control(int dutyCycle)
{
  shutPercentage = dutyCycle;

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
  TCNT1 = 0;                                                        // TIMER1 counter start value

  snprintf(s, S, "Waveform-H-bridge version: %s", VERSION);
  Serial.println(s);
  snprintf(s, S, "Electrical blocking percentage: %u%%", shutPercentage);
  Serial.println(s);
  calibrate(1, 1000000L);                                           // set initial TIMER1 compare values assuming zero phase difference
  Serial.println("Stabilizing...");

  // Offline execution time measurements of copied code block: takes 48 microseconds = 12 ticks
  // unsigned long observedMicros = micros();
  // unsigned long observedDiff = observedMicros - lastGpsMicros;
  // unsigned long observedTicks = observedDiff / TICK_MICROS;
  // int numWave = observedTicks / waveTicks;
  // unsigned long newHalfWave = 2 * numWave;
  // unsigned int newOCR1A;
  // unsigned int newTCNT1 = observedTicks - numWave * waveTicks;
  // if ((newTCNT1) >= ocr1aShut) {
  //   newHalfWave += 1;
  //   newTCNT1 -= ocr1aShut;
  //   newOCR1A = ocr1aOpen;
  // } else {
  //   newOCR1A = ocr1aShut;
  // }
  // OCR1A = newOCR1A;
  // TCNT1 = newTCNT1 + compensationTicks;
  // unsigned long time2 = micros();
  // Serial.println(observedMicros);
  // Serial.println(time2);
  compensationTicks = 12;
}

void run_shutter_control()
{
  if (!gpsHit) {
    return;
  }
  iGpsPulse++;                                                      // Overflow after 2^32 / 86400 / 365 = 136 years
  if (iGpsPulse == 0) {
    gpsStartMicros = lastGpsMicros;
  }
  if (iGpsPulse >= N_STABLE) {
    // Beware of concurrency issues; do not touch TIMER1 close to an ISR, so introduce a short delay if necessary
    // OCR1A is calculated such that avoidance should not happen during stable conditions
    unsigned int delayTicks = OCR1A - TCNT1;
    if (delayTicks < 50) {     // 50 an arbitrary number of ticks sufficient to execute the syncing further below
      delayMicroseconds((delayTicks + 8) * TICK_MICROS);
      if (iGpsPulse % 6 == 0) {
        snprintf(s, S, "Avoidance triggered! OCR1A: %u, TCNT1: %u", OCR1A, TCNT1);
        Serial.println(s);
      }
    }

    // Phase lock mechanisms 1 for the start of the pulse train each second (see explanation at top of file)
    // Phase of the pulse train: iHalfWave * OCR1A + TCNT1
    unsigned long oldIsr = iIsr;                                    // iIsr at time of phaseDiff measurement
    unsigned long oldHalfWave = iHalfWave;                          // for printing phaseDiff below
    unsigned int oldTCNT1 = TCNT1;                                  // for printing phaseDiff below
    // Start of code block for which execution time needs to be compensated
    unsigned long observedMicros = micros();                        // separate statement to allow for time measurement
    unsigned long observedDiff = observedMicros - lastGpsMicros;    // small value in lock state, depending on other tasks in loop()
    unsigned long observedTicks = observedDiff / TICK_MICROS;
    int numWave = observedTicks / waveTicks;
    unsigned long newHalfWave = 2 * numWave;
    unsigned int newOCR1A;
    unsigned int newTCNT1 = observedTicks - numWave * waveTicks;
    if ((newTCNT1) >= ocr1aShut) {
      newHalfWave += 1;
      newTCNT1 -= ocr1aShut;
      newOCR1A = ocr1aOpen;
    } else {
      newOCR1A = ocr1aShut;
    }
    OCR1A = newOCR1A;
    TCNT1 = newTCNT1 + compensationTicks;
    // End of code block for which execution time needs to be compensated
    iHalfWave = newHalfWave;

    // Log experienced phase difference to serial monitor
    if (iGpsPulse % 6 == 0) {
      oldHalfWave = oldHalfWave % N_HALF_WAVE;
      snprintf(s, S, "LCD phase: %lu %lu %lu %u", oldIsr, observedTicks, oldHalfWave, oldTCNT1);
      Serial.println(s);
    }

    // Preliminary calibration of the MCU clock against the GPS pulses
    unsigned long calibrationMicros = lastGpsMicros - gpsStartMicros;
    if (iGpsPulse == N_STABLE) {
      calibrate(N_STABLE, calibrationMicros);
    }

    // Periodoc calibration of the MCU clock against the GPS pulses
    // Successive calibration windows are used, longer rolling windows seems unnecessary
    if ( (iGpsPulse % N_CALIBRATE) == 0) {
      calibrate(N_CALIBRATE, calibrationMicros);
      gpsStartMicros = lastGpsMicros;
    }

    // Interrupt second markers if phase stability is too low (interference on GPS pulses)
    // Tested: the script recovers OK from a manually triggered unexpected GPS pulse
    // (pin D2 connected momentarily to GND via a test lead with a 100 Ohm resistor)
    long phaseDiff = lastGpsMicros - prevGpsMicros - calibratedFreq / MCU_MHZ;
    if (abs(phaseDiff) > 1000) {                                    // Arbitrary value
      iGpsPulse = N_INIT;
      Serial.println("Lock with GPS signal lost");
    }
  }

  // Optional highspeed logging for debugging
  #ifdef DEBUG_LOG
  if (iLog >= NLOG - 1 && iLog < 2 * NLOG - 2) {
    snprintf(s, S, "\nHispeed logging results preceding %lu\n", lastGpsMicros);
    Serial.println(s);
    for (int i=0; i < NLOG; i++) {
      snprintf(s, S, "iIsr: %lu iHalfWave: %lu  micros: %lu", logIsr[i], logHalf[i], logMicros[i]);
      Serial.println(s);
    }
  }
  #endif

  // Prepare for next 1 Hz GPS pulse
  prevGpsMicros = lastGpsMicros;
  gpsHit = false;
}
