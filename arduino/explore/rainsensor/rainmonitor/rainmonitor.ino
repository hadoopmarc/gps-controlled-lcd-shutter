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

#define SD_FAT_TYPE 1                   // For FAT16/FAT32
#define USE_LONG_FILE_NAMES 1
#define SPI_SPEED SD_SCK_MHZ(4)         // Can be max 50 MHz

const int8_t DISABLE_CHIP_SELECT = -1;  // Assume no other SPI devices present

// The LCD shutter PCB connects the Arduino pins to the RX/TX pins of the GPS module
const uint8_t RXPin = 10;               // Hardware connection on LCD shutter PCB
const uint8_t TXPin = 9;                // Hardware connection on LCD shutter PCB
const uint8_t chipSelect = 5;           // Hardware connection on LCD shutter PCB
const uint8_t rainPin = 6;              // Hardware connection on rainsensor shield

// Variables related to the measurement logic
uint8_t actionMinute;                   // Used to detect next minute cycle
uint8_t actionHour;                     // Used to detect next instance for GPS syncing
bool started = false;                   // Becomes true after first whole minute detection
const uint8_t nMeasure = 10;            // Number of measurements per minute
uint8_t iMeasure = 0;                   // Current measurement to be made
uint8_t actionSeconds[nMeasure];        // Used to detect next masurement instance
bool isWet[nMeasure];                   // Measured values from the past minute

String logFolder = "arduino";
String logFile = "";                    // "lat-lon-yymmdd.csv"
SdFat32 sd;
File32 testfile;

// void test_charops() {                // Probably more code-efficient than the String class
//   char t1[40] = "Hiep";
//   char t2[40] = "Hoi";
//   Serial.println(t1);
//   strcat(t1, t2);
//   Serial.println(t1);
// }

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
  logFile = setTimeWithGPS();
  SdFile::dateTimeCallback(populateDateTime);
  createFile(logFolder, logFile);
  time_t t = now();
  uint8_t waitMinutes;
  if (second(t) < 59) {
    waitMinutes = 1;
  } else {               // avoid race condition
    delay(1000);
    waitMinutes = 2;
  }
  actionMinute = (minute(t) + waitMinutes) % 60;
  Serial.println(F("Measuring started"));
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
      char str[40];
      snprintf(str, 40, "\n%4d-%02d-%02d %02d:%02d:%02d ",
        year(t), month(t), day(t), hour(t), minute(t), second(t));
      String line = String(str);
      for (int i=0; i < nMeasure; i++) {
        line += (int)isWet[i];
      }
      line += "\n";
      Serial.print(line);
      writeFile(logFile, line);
    }
    iMeasure = 0;
  }

  if (hour(t) == actionHour) {
    setTimeWithGPS();
  }
}

String setTimeWithGPS() {
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
  gpsSerial.println(F("$PUBX,40,VTG,0,0,0,0*5E"));  // VTG OFF
  gpsSerial.println(F("$PUBX,40,GGA,0,0,0,0*5A"));  // GGA OFF
  gpsSerial.println(F("$PUBX,40,GSA,0,0,0,0*4E"));  // GSA OFF
  gpsSerial.println(F("$PUBX,40,GSV,0,0,0,0*59"));  // GSV OFF
  gpsSerial.println(F("$PUBX,40,GLL,0,0,0,0*5C"));  // GLL OFF
  while (gpsSerial.available()) {}                  // Clear buffer

  // delay(2000);                                   // Wait for complete GNRMC message
  // int i = 0;
  // while (gpsSerial.available() && i < 200) {
  //   char c = gpsSerial.read();
  //   Serial.print(c);
  // }
  // Serial.println();
  // while (!gpsSerial.available()) {}

  // Displays GNRMC messages with time in UTC, see:
  //     https://logiqx.github.io/gps-wizard/nmea/messages/rmc.html
  // Steps:
  // - loop until '$'
  // - loop until the first ','
  // - assign the next 6 bytes to the gpsTime char array
  // - loop until the ninth ','
  // - assign the next 6 bytes to the gpsDate char array
  // - analogously for latitude and longitude
  String gpsTime = "000000";
  String gpsDate = "000000";
  String latitude = "0000";                         // ddmm  North/South not read
  String longitude = "00000";                       // dddmm  East/West not read
  char current;
  bool lineStarted = false;
  int iComma;
  int iCopy;
  while (!gpsSerial.available()) {}                 // Wait for start of GNRMC message
  while (gpsSerial.available() > 0) {
    current = gpsSerial.read();
    delay(5);                                       // Do not read too fast
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
      gpsTime.setCharAt(iCopy, current);
      iCopy++;
    }
    if (iComma == 3 && iCopy < 4) {
      latitude.setCharAt(iCopy, current);
      iCopy++;
    }
    if (iComma == 5 && iCopy < 5) {
      longitude.setCharAt(iCopy, current);
      iCopy++;
    }
    if (iComma == 9) {
      gpsDate.setCharAt(iCopy, current);
      iCopy++;
      if (iCopy == 6) {
        break;                                      // Parsing completed
      }
    }
  }
  char space = ' ';
  gpsSerial.end();
  Serial.print('\n');
  Serial.print(latitude);
  Serial.print(space);
  Serial.print(longitude);
  Serial.print(space);
  Serial.print(gpsDate);
  Serial.print(space);
  Serial.println(gpsTime);
  actionHour = (uint8_t)(gpsTime.substring(0, 2).toInt());
  setTime(
    actionHour,
    gpsTime.substring(2, 4).toInt(),
    gpsTime.substring(4, 6).toInt(),
    gpsDate.substring(0, 2).toInt(),
    gpsDate.substring(2, 4).toInt(),
    gpsDate.substring(4, 6).toInt() + 2000
  );
  actionHour = (actionHour + 1) % 24;
  if (logFile == "") {
    logFile = logFolder + "/" + latitude + "-" + longitude + "-"
      + gpsDate.substring(4, 6) + gpsDate.substring(2, 4) + gpsDate.substring(0, 2)
      + ".csv";
    Serial.print("Logfile: ");
    Serial.println(logFile);
  }
  return logFile;
}

void populateDateTime(uint16_t* date, uint16_t* time) {
  // From: https://github.com/greiman/SdFat/blob/2.2.3/doc/html.zip
  time_t t = now();
  *date = FS_DATE(year(t), month(t), day(t));
  *time = FS_TIME(hour(t), minute(t), second(t));
}

void createFile(String folder, String filename) {
  // O_flags, see: https://github.com/greiman/SdFat/blob/2.2.3/src/FsLib/FsFile.h#L450
  sd.begin(chipSelect, SPI_SPEED);
  sd.mkdir(folder.c_str());
  sd.ls(LS_R | LS_DATE | LS_SIZE);
  testfile.open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
  testfile.close();
  sd.end();
}

void writeFile(String filename, String line) {
  sd.begin(chipSelect, SPI_SPEED);
  testfile.open(filename.c_str(), O_WRONLY | O_APPEND);
  testfile.write(line.c_str());
  testfile.close();
  sd.end();
}