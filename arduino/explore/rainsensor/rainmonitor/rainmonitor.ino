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

// Variables related to the measurement logic
uint8_t actionMinute;                   // Used to to detect next minute cycle
bool started = false;                   // Becomes true after first whole minute detection
const uint8_t nMeasure = 10;            // Number of measurements per minute
uint8_t iMeasure = 0;                   // Current measurement to be made
uint8_t actionSeconds[nMeasure];        // Used to detect next masurement instance
bool isWet[nMeasure];                   // Measured values from the past minute

const int S = 90;
char s[S];


void setup() {
  Serial.begin(9600);

  // configure pin D6 as an input and enable the internal pull-up resistor
  // https://docs.arduino.cc/tutorials/generic/digital-input-pullup/)
  pinMode(rainPin, INPUT_PULLUP);

  // Precalculate actionMinutes
  for (int i=0; i<nMeasure; i++) {
    actionSeconds[i] = (i + 1) * 59 / nMeasure;
  }

  // Synchronize internal clock with GPS time
  // ToDo: should be done once per day for prolonged measurements
  if (setTimeWithGPS()) {
    Serial.println("Failure reading data from GPS");
  }
  SdFile::dateTimeCallback(populateDateTime);
  createFile("geen_metingen2.csv");
  time_t t = now();
  uint8_t waitMinutes;
  if (second(t) < 59) {
    waitMinutes = 1;
  } else {               // avoid race condition
    delay(1000);
    waitMinutes = 2;
  }
  actionMinute = (minute(t) + waitMinutes) % 60;
  Serial.println("Measuring started");
}

void loop() {
  time_t t = now();

  // Measure
  if (second(t) == actionSeconds[iMeasure]) {
    // Input LOW means:  sensor relay closed -> wet conditions -> need ~LOW
    // Input HIGH means: sensor relay open -> dry conditions -> need ~HIGH
    isWet[iMeasure] = !digitalRead(rainPin);
    Serial.print(".");
    iMeasure++;
  }

  // Process past measurements at the start of a minute
  if (minute(t) == actionMinute) {
    actionMinute = (actionMinute + 1) % 60;
    if (iMeasure == nMeasure) {
      snprintf(s, S, "\n%4d-%02d-%02d %02d:%02d:%02d ",
        year(t), month(t), day(t), hour(t), minute(t), second(t));
      Serial.print(s);
      for (int i=0; i < nMeasure; i++) {
        snprintf(s, S, "%d", isWet[i]);
        Serial.print(s);
      }
      Serial.println("");
    }
    iMeasure = 0;
  }
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
  delay(2000);                                      // wait for complete GNRMC message

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
  int iComma;
  int iCopy;
  while (gpsSerial.available() > 0) {
    current = gpsSerial.read();
    delay(1);                   // Do not read too fast
    if (!lineStarted) {
      if (current == '\r') {
        lineStarted = true;
        iComma = 0;
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
  Serial.println("\n" + gpsDate + " " + gpsTime);
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
