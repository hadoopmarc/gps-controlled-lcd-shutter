/* Based on:
 * https://lastminuteengineers.com/neo6m-gps-arduino-tutorial/
 * https://forum.arduino.cc/t/configurating-ublox-gps-on-bootup-from-arduino/903699/9
 * https://www.hhhh.org/wiml/proj/nmeaxor.html  NMEA message checksum calculator
 *
 * This tests the serial readout of a GPS module during long periods. It flashes the onboard LED
 * to demonstrate that the main loop is still functioning when no serial monitor is connected.
 *
 * SoftwareSerial is infamous for all the trouble it can result in. Read its history at:
 * https://www.martyncurrey.com/arduino-serial-a-look-at-the-different-serial-libraries/
 * And indeed, after long and frustrating tests it turned out:
 * - when the USB is detached and the board gets a cold boot, Serial and SoftwareSerial should not
 *   be active together, otherwise SoftwareSerial hangs (so any use of either should be started
 *   with begin() and finished with end())
 * - the problem does not occur when the board is started with the USB connected and then disconnected
 * - the problem does not occur when the board is started with the USB connected, then disconnected
 *   and then is reset (warm reboot)
 * - when the older NeoSWSerial is used instead of the IDE 2.x SoftwareSerial, things are even worse and
 *   Serial should never be used for SoftwareSerial to keep working.
 */

#include <SoftwareSerial.h>

// The two Arduino pins hardwired on the GPS LCD shutter PCB
int RXPin = 10;
int TXPin = 9;

int LEDPin = 13;
SoftwareSerial gpsSerial(RXPin, TXPin);

void setup()
{
  pinMode(LEDPin, OUTPUT);
}

void loop()
{
  setTimeWithGPS();
  // Visual indication of running loop during standalone operation
  digitalWrite(LEDPin, HIGH);
  delay(200);
  digitalWrite(LEDPin, LOW);
  delay(2000);
}

void setTimeWithGPS() {
  /*
   * SoftwareSerial uses the same hardware timer as the LCD shutter control, so only call this
   * function at daytime when accurate LCD shutter control is not required.
   *
   * Based on:
   * https://lastminuteengineers.com/neo6m-gps-arduino-tutorial/
   * https://forum.arduino.cc/t/configurating-ublox-gps-on-bootup-from-arduino/903699/9
   * https://www.hhhh.org/wiml/proj/nmeaxor.html  NMEA message checksum calculator
   */
  gpsSerial.begin(9600);                            // Default baudrate of NEO GPS modules

  // Quiet unnecessary messages before the first second has passed
  gpsSerial.println(F("$PUBX,40,VTG,0,0,0,0*5E"));  // VTG OFF
  gpsSerial.println(F("$PUBX,40,GGA,0,0,0,0*5A"));  // GGA OFF
  gpsSerial.println(F("$PUBX,40,GSA,0,0,0,0*4E"));  // GSA OFF
  gpsSerial.println(F("$PUBX,40,GSV,0,0,0,0*59"));  // GSV OFF
  gpsSerial.println(F("$PUBX,40,GLL,0,0,0,0*5C"));  // GLL OFF

  // Displays GNRMC messages with time in UTC, see:
  //     https://logiqx.github.io/gps-wizard/nmea/messages/rmc.html
  // Steps:
  // - loop until '$'
  // - loop until the first ','
  // - assign the next 6 bytes to the gpsTime char array
  // - loop until the ninth ','
  // - assign the next 6 bytes to the gpsDate char array
  // - analogously for latitude and longitude
  //
  // Reading the 64 byte ring buffer of SoftwareSerial is a bit tricky, see:
  // https://arduino.stackexchange.com/questions/1726/how-does-the-arduino-handle-serial-buffer-overflow
  // Basically, doing it right involves the following steps:
  // - clear the buffer from old data
  // - loop fast and check every time whether a character is available
  // - the pace of the 9600 baud serial interface is 1 byte every 1.04 ms
  char gpsTime[7] = "000000";
  char gpsDate[7] = "000000";
  char latitude[5] = "0000";                        // ddmm  North/South not read
  char longitude[6] = "00000";                      // dddmm  East/West not read
  char current;
  bool lineStarted = false;
  int iComma = 0;
  int iCopy;
  delay(1100);
  while (gpsSerial.available()) {                   // Clear buffer from old data
    gpsSerial.read();
  }
  while (true) {
    if (gpsSerial.available()) {                    // Loop fast until a char is available
      current = gpsSerial.read();
    } else {
      continue;
    }
    if (!lineStarted) {
      if (current == '$') {
        lineStarted = true;
        iComma = 0;
      }
      continue;
    }
    if (current == ',') {
      iComma++;
      iCopy = 0;
      continue;
    }
    if (iComma == 1 && iCopy < 6) {
      gpsTime[iCopy] = current;
      iCopy++;
    }
    if (iComma == 3 && iCopy < 4) {
      latitude[iCopy] = current;
      iCopy++;
    }
    if (iComma == 5 && iCopy < 5) {
      longitude[iCopy] = current;
      iCopy++;
    }
    if (iComma == 9) {
      gpsDate[iCopy] = current;
      iCopy++;
      if (iCopy == 6) {
        break;
      }
    }
  }
  char space = ' ';
  gpsSerial.end();
  Serial.begin(9600);
  Serial.print(millis() / 1000);
  Serial.print(": ");
  Serial.print(latitude);
  Serial.print(space);
  Serial.print(longitude);
  Serial.print(space);
  Serial.print(gpsDate);
  Serial.print(space);
  Serial.println(gpsTime);
  Serial.end();
}
 