/* 
 * Simple rain monitor using a B+B Thermo-Technik 2020 version rain sensor
 *
 * Sensor box has default factory settings for the internal jumpers ("plug links")
 * (heating on; close the relay when wet conditions occur)
 *
 * Connections with 3-wire cable:
 * - blue: GND from Arduino to ground and REL CO of rain sensor
 * - brown: +12 V from Vin Arduino to rain sensor
 * - green/yellow: D2 from Arduino to REL NO of rain sensor
 */
// Time Keeping
// - gpsSerial gives current date and time; set calibrationDate and currentTime
// - gpsIn gives second pulses; increment currentTime
// - date and time are needed for the populateDatetime callback of SdFat
// - date and time are needed for writing log lines; date and time functions from SdFast
//   are not easily reused.
#include <SdFat.h>
#include <sdios.h>
#include <SoftwareSerial.h>
#include <SPI.h>

#define SD_FAT_TYPE 1                   // For FAT16/FAT32
#define USE_LONG_FILE_NAMES 1           // For encoding lat, lon and date
#define SPI_SPEED SD_SCK_MHZ(4)         // Can be max 50 MHz
const int8_t DISABLE_CHIP_SELECT = -1;  // Assume no other SPI devices present

// GPS module hardware configs (hardwired on the PCB of the LCD shutter)
const uint8_t gpsPin = 2;               // Hardware connection on LCD shutter PCB
const uint8_t RXPin = 10;               // Hardware connection on LCD shutter PCB
const uint8_t TXPin = 9;                // Hardware connection on LCD shutter PCB
const uint8_t chipSelect = 5;           // Hardware connection on LCD shutter PCB

// Rainsensor hardware configs
const uint8_t rainPin = 6;              // Hardware connection on the rainsensor shield

// Global variables modified in interrupt routines
volatile bool gpsHit;                   // Set by the gpsIn interrupt only and cleared after processing

// Variables related to the measurement logic
bool isCalibrated;                      // Used to detect if daily date calibration was done
uint8_t actionMinute;                   // Used to detect next minute cycle
bool started = false;                   // Becomes true after first whole minute detection
const uint8_t nMeasure = 10;            // Number of measurements per minute
uint8_t iMeasure = 0;                   // Current measurement to be made
uint8_t actionSeconds[nMeasure];        // Used to detect next masurement instance
char isWet[nMeasure];                   // Measured values from the past minute as 00001000 char array

char logFolder[] = "rain";
char logFile[] = "rain/ddmm-dddmm-yymmdd.csv";    // "lat-lon-date.csv"
SoftwareSerial gpsSerial(RXPin, TXPin);
SdFat32 sd;
File32 testfile;

struct { 
  uint8_t day;
  uint8_t month;
  uint16_t year;                        // Century included
} calibrationDate;

struct { 
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
} currentTime;

void incrementTime() {
  currentTime.second = (currentTime.second + 1) % 60;
  if (currentTime.second == 0) {
    currentTime.minute = (currentTime.minute + 1) % 60;
    if (currentTime.minute == 0) {
      currentTime.hour = (currentTime.hour + 1) % 24;
    }
  }
}

void gpsIn()
{
  gpsHit = true;
}

void setup() {
  Serial.begin(9600);

  // Receive second pulses from GPS
  attachInterrupt(digitalPinToInterrupt(gpsPin), gpsIn, RISING);

  // configure the conducting rain sensor as an input and enable the internal pull-up resistor
  // https://docs.arduino.cc/tutorials/generic/digital-input-pullup/)
  pinMode(rainPin, INPUT_PULLUP);

  // Precalculate actionMinutes
  for (int i=0; i<nMeasure; i++) {
    actionSeconds[i] = (i + 1) * 59 / nMeasure;
  }

  gpsHit = false;
  while (!gpsHit) {            // GPS fix needed for log filename and measure times
    Serial.println("Waiting for gps fix");
    delay(10000);
  }
  gpsHit = false;
  setGpsDependentVariables();  // Sets calibrationDate, currentTime and logFile
  createFile(logFolder, logFile);

  uint8_t waitMinutes;
  if (currentTime.second < 59) {
    waitMinutes = 1;
  } else {                     // avoid race condition
    delay(1000);
    waitMinutes = 2;
  }
  actionMinute = (currentTime.minute + waitMinutes) % 60;
  Serial.println(F("Measuring started"));
}

void loop() {
  // Keep current time
  if (gpsHit) {
    gpsHit = false;
    incrementTime();
  }

  // Measure
  if (currentTime.second == actionSeconds[iMeasure]) {
    // Input LOW means:  sensor relay closed -> wet conditions -> need ~LOW
    // Input HIGH means: sensor relay open -> dry conditions -> need ~HIGH
    isWet[iMeasure] = (digitalRead(rainPin)) ? '0': '1';
    Serial.print(".");
    iMeasure++;
  }

  // Process past measurements at the start of a minute
  if (currentTime.minute == actionMinute) {
    actionMinute = (actionMinute + 1) % 60;
    if (iMeasure == nMeasure) {
      char line[40];
      snprintf(line, 10, "%02d:%02d:%02d ",
               currentTime.hour, currentTime.minute, currentTime.second);
      memcpy(line + 9, isWet, nMeasure);
      uint8_t writePos = 9 + nMeasure;
      line[writePos] = '\n';
      writePos++;
      line[writePos] = '\0';
      Serial.print(line);
      writeFile(logFile, line);
    }
    iMeasure = 0;
  }

  // Recalibrate and start new log file at noon or later if not done for the current day
  if (!isCalibrated && currentTime.hour == 12) {  // Occurs every noon during continuous operation
    setGpsDependentVariables();
    createFile(logFolder, logFile);
  }
  if (isCalibrated && currentTime.hour == 0) {    // Prepare for recalibration + logFile creation next noon
    isCalibrated = false;
  }
}

void setGpsDependentVariables() {
  /*
   * SoftwareSerial uses the same hardware timer as the LCD shutter control, so only call this
   * function at daytime when accurate LCD shutter control is not required.
   *
   * Based on:
   * https://lastminuteengineers.com/neo6m-gps-arduino-tutorial/
   * https://forum.arduino.cc/t/configurating-ublox-gps-on-bootup-from-arduino/903699/9
   * https://www.hhhh.org/wiml/proj/nmeaxor.html  NMEA message checksum calculator
   */
  Serial.end();                                     // Serial and gpsSerial depend on same hardware timers
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
  char gpsTime[7] = "000000";                       // hhmmss
  char gpsDate[7] = "000000";                       // ddmmyy
  char latitude[5] = "0000";                        // ddmm, North/South not read
  char longitude[6] = "00000";                      // dddmm, East/West not read
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
  Serial.print(latitude);
  Serial.print(space);
  Serial.print(longitude);
  Serial.print(space);
  Serial.print(gpsDate);
  Serial.print(space);
  Serial.println(gpsTime);

  memcpy(logFile + 5, latitude, 4);
  memcpy(logFile + 10, longitude, 5);
  memcpy(logFile + 16, gpsDate, 6);
  Serial.print("Logfile: ");
  Serial.println(logFile);

  calibrationDate.day = (uint8_t)atoi(gpsDate + 4);
  gpsDate[4] = '\0';
  calibrationDate.month = (uint8_t)atoi(gpsDate + 2);
  gpsDate[2] = '\0';
  calibrationDate.year = (uint16_t)(atoi(gpsDate) + 2000);

  currentTime.second = (uint8_t)atoi(gpsTime + 4);
  gpsTime[4] = '\0';
  currentTime.minute = (uint8_t)atoi(gpsTime + 2);
  gpsTime[2] = '\0';
  currentTime.hour = (uint8_t)atoi(gpsTime);

  isCalibrated = true;
}

void populateDateTime(uint16_t* date, uint16_t* time) {
  // Purpose: give SdFat access to current date and time
  // See: https://github.com/greiman/SdFat/blob/2.2.3/doc/html.zip
  *date = FS_DATE(calibrationDate.year, calibrationDate.month, calibrationDate.day);
  *time = FS_TIME(currentTime.hour, currentTime.minute, currentTime.second);
}

void createFile(char *folder, char *filename) {
  // O_flags, see: https://github.com/greiman/SdFat/blob/2.2.3/src/FsLib/FsFile.h#L450
  sd.begin(chipSelect, SPI_SPEED);
  sd.mkdir(folder);
  sd.ls(LS_R | LS_DATE | LS_SIZE);
  SdFile::dateTimeCallback(populateDateTime);
  testfile.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
  testfile.close();
  sd.end();
}

void writeFile(char *filename, char *line) {
  sd.begin(chipSelect, SPI_SPEED);
  SdFile::dateTimeCallback(populateDateTime);
  testfile.open(filename, O_WRONLY | O_APPEND);
  testfile.write(line);
  testfile.close();
  sd.end();
}
