/***************************************************
  This is a library example for the MLX90614 Temp Sensor

  Designed specifically to work with the MLX90614 sensors in the
  adafruit shop
  ----> https://www.adafruit.com/products/1747 3V version
  ----> https://www.adafruit.com/products/1748 5V version

  These sensors use I2C to communicate, 2 pins are required to
  interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ****************************************************/
/* 
  Wiring on Arduino Uno/Nano:
    SDA: A4
    SCL: A5
    Vin: 3V3
    GND: GND

  Example source:
    https://github.com/adafruit/Adafruit-MLX90614-Library/blob/master/examples/mlxtest/mlxtest.ino
 */
#include <SdFat.h>
#include <SoftwareSerial.h>
#include <Adafruit_MLX90614.h>

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// !!! Part of inserted provocation test
#define SPI_SPEED SD_SCK_MHZ(4)         // Can be max 50 MHz
const uint8_t rxPin = 10;               // Hardware connection on LCD shutter PCB
const uint8_t txPin = 9;                // Hardware connection on LCD shutter PCB
const uint8_t chipSelect = 5;           // Hardware connection on LCD shutter PCB
SoftwareSerial gpsSerial(rxPin, txPin);
// !!!


void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Adafruit MLX90614 test");

  // !!! Part of inserted provocation test
  writeFile("hiep", "hoi");
  if (!mlx.begin()) {
    Serial.println("Error connecting to MLX sensor. Check wiring.");
    while (1);
  };
  pinMode(rxPin, INPUT_PULLUP);  // The sdfat library or one of its dependencies interferes with this setting
  setGpsDependentVariables();
  // !!!

  Serial.print("Emissivity = "); Serial.println(mlx.readEmissivity());
  Serial.println("================================================");
}

void loop() {
  Serial.print("Ambient = "); Serial.print(mlx.readAmbientTempC());
  Serial.print("*C\tObject = "); Serial.print(mlx.readObjectTempC()); Serial.println("*C");
  Serial.print("Ambient = "); Serial.print(mlx.readAmbientTempF());
  Serial.print("*F\tObject = "); Serial.print(mlx.readObjectTempF()); Serial.println("*F");

  Serial.println();
  delay(5000);
}

// !!! Part of inserted provocation test
void writeFile(char *filename, char *line) {
  // O_flags, see: https://github.com/greiman/SdFat/blob/2.2.3/src/FsLib/FsFile.h#L450
  SdFat32 sd;
  File32 testfile;
  sd.begin(chipSelect, SPI_SPEED);
  testfile.open(filename, O_WRONLY | O_CREAT | O_APPEND);
  testfile.write(line);
  testfile.close();
  sd.end();
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
  char gpsTime[] = "000000";                       // hhmmss
  char gpsDate[] = "000000";                       // ddmmyy
  char latitude[] = "0000";                        // ddmm, North/South not read
  char longitude[] = "00000";                      // dddmm, East/West not read
  char current;
  bool lineStarted = false;
  int iComma = 0;
  int iCopy;
  delay(1100);
  while (gpsSerial.available()) {                   // Clear buffer from old data
    gpsSerial.read();
  }
  // // !!! part of temp provocation test
  // gpsSerial.end();
  // Serial.begin(9600);
  // Serial.println("... gps first available passed");
  // Serial.end();
  // gpsSerial.begin(9600);
  // // !!!
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
}
// !!!
