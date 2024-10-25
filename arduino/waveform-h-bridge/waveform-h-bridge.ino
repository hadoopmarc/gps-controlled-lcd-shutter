/*
 * Arduino sketch entrypoint that calls gps_shutter_control and possibly other tasks
 * for operating a meteor photography allsky station.
 */

// Percentage of time that the LCD shutter receives a voltage (user editable in range [15, 70])
// Advised value for standard grade LCtec shutters:   25
// Advised value for fast X-grade LCtec shutters:     50
#define SHUT_PERCENTAGE 25

#include "gps_shutter_control.h"

void setup()
{
  // For writing log messages to the serial console of the Arduino IDE
  // Keep the baudrate low to prevent large numbers of interrupts from the serial interface
  Serial.begin(9600);

  // Put setup logic for other control tasks here
  // ...

  // Keep this as final statement of the setup() function
  setup_shutter_control(SHUT_PERCENTAGE);
}

void loop()
{
  // Put other control tasks here, but keep execution time per iteration within 20 milliseconds
  // ...

  // Call the LCD shutter control each iteration of the loop
  run_shutter_control();
}

// Only edit if you know what you are doing
#if SHUT_PERCENTAGE < 15
#error "SHUT_PERCENTAGE should be larger or equal than 15"
#endif
#if SHUT_PERCENTAGE > 70
#error "SHUT_PERCENTAGE should be smaller or equal than 70"
#endif