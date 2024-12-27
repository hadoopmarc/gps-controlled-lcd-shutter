/* 
 * Simple rain monitor using a B+B Thermo-Technik 2020 version rain sensor
 *
 * Sensor box has plug links as the factory setting
 * (with heating on; close the relay when wet conditions occur)
 *
 * Current 3-wire cable:
 * - blue: GND from Arduino to ground and REL CO of rain sensor
 * - brown: +12 V from Vin Arduino to rain sensor
 * - green/yellow: D2 from Arduino to REL NO of rain sensor
 */
#include <SdFat.h>
#include <sdios.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <TimeLib.h>                    // https://github.com/PaulStoffregen/Time

#define SPI_SPEED SD_SCK_MHZ(4)         // Should be max 50 MHz

const int8_t DISABLE_CHIP_SELECT = -1;  // Assume no other SPI devices present
// The LCD shutter PCB connects the Arduino pins to the RX/TX pins of the GPS module
const uint8_t RXPin = 10;               // Hardware connection on LCD shutter PCB
const uint8_t TXPin = 9;                // Hardware connection on LCD shutter PCB
const uint8_t chipSelect = 5;           // Hardware connection on LCD shutter PCB
const uint8_t rainPin = 6;              // Hardware connection on rainsensor shield


void setup() {
  Serial.begin(9600);

  // configure pin D6 as an input and enable the internal pull-up resistor
  // https://docs.arduino.cc/tutorials/generic/digital-input-pullup/)
  pinMode(rainPin, INPUT_PULLUP);

  // Synchronize internal clock with GPS time
  if (setTimeWithGPS()) {
    Serial.println("Failure reading data from GPS");
  }
  delay(1000);
  createFile("geen_metingen2.csv");
}

void loop() {
  int S = 90;
  char s[S];

  time_t t = now();
  snprintf(s, S, "%4d-%02d-%02d %02d:%02d:%02d ",
    year(t), month(t), day(t), hour(t), minute(t), second(t));
  Serial.print(s);
  // Input LOW means:  sensor relay closed -> wet conditions
  // Input HIGH means: sensor relay open -> dry conditions
  int sensorVal = digitalRead(rainPin);

  if (sensorVal == HIGH) {
    Serial.println("Dry");
  } else {
    Serial.println("Wet");
  }

  delay(10000);
}

bool setTimeWithGPS() {
  /*
   * Only call this function at daytime when accurate LCD shutter control is not required
   *
   * Based on:
   * https://lastminuteengineers.com/neo6m-gps-arduino-tutorial/
   * https://forum.arduino.cc/t/configurating-ublox-gps-on-bootup-from-arduino/903699/9
   * https://www.hhhh.org/wiml/proj/nmeaxor.html  NMEA message checksum calculator
   */
  SoftwareSerial gpsSerial(RXPin, TXPin);
  gpsSerial.begin(9600);                            // Default baudrate of NEO GPS modules

  // Quiet unnecessary messages before the first second has passed
  // ToDo: removing F macro puts strings in RAM rather than flash. Use print formatting.
  gpsSerial.println(F("$PUBX,40,VTG,0,0,0,0*5E"));  // VTG OFF
  gpsSerial.println(F("$PUBX,40,GGA,0,0,0,0*5A"));  // GGA OFF
  gpsSerial.println(F("$PUBX,40,GSA,0,0,0,0*4E"));  // GSA OFF
  gpsSerial.println(F("$PUBX,40,GSV,0,0,0,0*59"));  // GSV OFF
  gpsSerial.println(F("$PUBX,40,GLL,0,0,0,0*5C"));  // GLL OFF
  delay(5000);                                      // wait for message completion
  // Displays GNRMC messages with time in UTC, see:
  //     https://logiqx.github.io/gps-wizard/nmea/messages/rmc.html
  // Steps:
  // - loop until <CR><LF> alias '\r''\n'
  // - loop until the first ','
  // - assign the next 6 bytes to the gpsTime char array
  // - loop until the ninth ','
  // - assign the next 6 bytes to the gpsDate char array
  String gpsTime = "000000";
  String gpsDate = "000000";
  char current;
  bool lineStarted = false;
  int iComma = 0;
  int iCopy;
  while (gpsSerial.available() > 0) {
    current = gpsSerial.read();
    if (!lineStarted) {
      if (current == '\r') {
        if (gpsSerial.available() > 0 && gpsSerial.read() == '\n') {
          lineStarted = true;
        }
        continue;
      }
    }
    if (current == ',') {
      if (iComma == 1 && iCopy != 6) {
        return 1;
      }
      iComma++;
      iCopy = 0;
      continue;
    }
    if (iComma == 1 && iCopy < 6) {
      gpsTime.setCharAt(iCopy, current);
      iCopy++;
    }
    if (iComma == 9) {
      gpsDate.setCharAt(iCopy, current);
      iCopy++;
      if (iCopy == 6) {
        break;  // parsing of time and date completed
      }
    }
  }
  gpsSerial.end();
  Serial.println(iCopy);
  Serial.println(gpsDate);
  Serial.println(gpsTime);
  if (iCopy != 6) {
    return 1;
  }
  setTime(
    gpsTime.substring(0, 2).toInt(),
    gpsTime.substring(2, 4).toInt(),
    gpsTime.substring(4, 6).toInt(),
    gpsDate.substring(0, 2).toInt(),
    gpsDate.substring(2, 4).toInt(),
    gpsDate.substring(4, 6).toInt() + 2000
  );
  return 0;
}

void createFile(char *filename)
{
  SdFat32 sd;
  SdFile sdf;
  sdf.dateTimeCallback(populateDateTime);
  sd.begin(chipSelect, SPI_SPEED);
  File32 testfile;
  // O_flags, see: https://github.com/greiman/SdFat/blob/2.2.3/src/FsLib/FsFile.h#L450
  testfile.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
  testfile.write( "See if this works!");
  testfile.close();

  Serial.println("Files found (date time size name):");
  sd.ls(LS_R | LS_DATE | LS_SIZE);
  sd.end();
}

void populateDateTime(uint16_t* date, uint16_t* time) {
  // From: https://github.com/greiman/SdFat/blob/2.2.3/doc/html.zip
  time_t t = now();
  *date = FS_DATE(year(t), month(t), day(t));
  *time = FS_TIME(hour(t), minute(t), second(t));
}
