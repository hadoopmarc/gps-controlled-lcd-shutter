/*
 * Arduino sketch entrypoint that calls gps_shutter_control and possibly other tasks
 * for operating a meteor photography allsky station.
 */
#include "gps_shutter_control.h"

void setup()
{
  // For writing log messages to the serial console of the Arduino IDE
  // Keep the baudrate low to prevent large numbers of interrupts from the serial interface
  Serial.begin(9600);

  // Put setup logic for other control tasks here
  // ...

  // Keep this as final statement of the setup() function
  setup_shutter_control();
}

void loop()
{
  // Put other control tasks here, but keep execution time per iteration within 50 millisecond
  // ...

  // Call the LCD shutter control each iteration of the loop
  run_shutter_control();
}
