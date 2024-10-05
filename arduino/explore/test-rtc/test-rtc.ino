// Based on:
// https://lastminuteengineers.com/neo6m-gps-arduino-tutorial/
// https://forum.arduino.cc/t/configurating-ublox-gps-on-bootup-from-arduino/903699/9
// https://www.hhhh.org/wiml/proj/nmeaxor.html  NMEA message checksum calculator
#include <SoftwareSerial.h>

// Choose two Arduino pins to use for software serial
int RXPin = 10;
int TXPin = 9;


void setup()
{
  // Start the Arduino hardware serial port at 9600 baud
  Serial.begin(9600);
  if (setTimesWithGPS()) {
    Serial.println("Failure reading data from GPS");
  }
}


void loop()
{
  delay(10);
}


bool setTimesWithGPS() {
  /*
   *  Only call this funtion outside the time window that needs accurate LCD shutter control
   */
  SoftwareSerial gpsSerial(RXPin, TXPin);
  gpsSerial.begin(9600);                            // Default baudrate of NEO GPS modules

  // Quiet unnecessary messages before the first second has passed
  gpsSerial.println(F("$PUBX,40,VTG,0,0,0,0*5E"));  // VTG OFF
  gpsSerial.println(F("$PUBX,40,GGA,0,0,0,0*5A"));  // GGA OFF
  gpsSerial.println(F("$PUBX,40,GSA,0,0,0,0*4E"));  // GSA OFF
  gpsSerial.println(F("$PUBX,40,GSV,0,0,0,0*59"));  // GSV OFF
  gpsSerial.println(F("$PUBX,40,GLL,0,0,0,0*5C"));  // GLL OFF
  delay(5000);                                      // wait for message completion
  // Displays GNRMC messages with time in UTC, see:
  //     https://logiqx.github.io/gps-wizard/nmea/messages/rmc.html
  if (gpsSerial.available() > 20) {
    while (gpsSerial.available() > 0)
      Serial.write(gpsSerial.read());
      Serial.println(" ");
    gpsSerial.end();
    return 0;
  } else {
    return 1;
  }
}
