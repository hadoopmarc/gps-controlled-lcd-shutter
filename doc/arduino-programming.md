# Arduino programming

Programming the Arduino Nano can already be done before it is mounted on the PCB, but it is also possible to do it afterwards. Also, future updates of the software can be uploaded without removing the Arduino from the PCB. Once the Arrduino is programmed, the program is stored in its flash memory. Whenever you restart the Arduino, it will run the program from the flash memory.

## Installing the Arduino IDE

The Arduino IDE is available for all popular operating systems. You install it from one of the package managers supported by your OS or you can install it from the [Arduino download page](https://www.arduino.cc/en/software), whatever you trust better. Some OS may require to install the Java Runtime Enviromnemt first. Also here, install it from one of the package managers or get the full JDK17 from [https://www.oracle.com/java/technologies/downloads/](https://www.oracle.com/java/technologies/downloads/).

Most likely you use Microsoft Windows and you can simply install the Arduino IDE legacy version (1.8.x) from the Microsoft Store, which includes a copy of the Java JRE. The Arduino program in this git repository was only tested with this legacy version.

## Downloading the LCD shutter driving software

The latest software for the GPS-controlled LCD shutter driver can be downloaded from:

[https://github.com/hadoopmarc/gps-controlled-lcd-shutter/archive/refs/heads/main.zip](https://github.com/hadoopmarc/gps-controlled-lcd-shutter/archive/refs/heads/main.zip)

Extract the archive at some place where it can easily be found back. Of course, if you know how to use git and want to contribute to the repository, you can git clone the repository instead of donwloading the archive.

## Programming the Arduino

Programming the Arduino Nano should now be so simple as:

1. connect the Arduino to your PC/laptop using a USB-cable
2. open the local file 'gps-controlled-lcd-shutter/arduino/waveform-h-bridge/waveform-h-bridge.ino' with the Arduino IDE. This will open three files in separate tabs. The file gps_shutter_control.cpp contains the actual program. The two other files prepare the Arduino for later extensions.
3. select the Arduino Nano from the "Tools / Board / Arduino AVR boards" menu option
4. select the right COM-port from the "Tools / Port" menu option (if no COM port is marked as Arduino Nano, disconnect and reconnect the Arduino to discover the right COM port)
5. push the "Upload ->" button in the taskbar. If the compilation fails, please contact the repository owner. If the upload fails, disconnect and reconnect the Arduino and try again.

The microcontroller on an Arduino board may have so-called [lock bits](https://microchip.my.site.com/s/article/Use-of-Lock-Bits-in-AVR-devices)or [fuses](https://circuitdigest.com/microcontroller-projects/understanding-fuse-bits-in-atmega328p-to-enhance-arduino-programming-skills) set such that it cannot be programmed with the Arduino IDE anymore. The lockbits can be reset by following the [Arduino bootloader burning procedure](https://docs.arduino.cc/built-in-examples/arduino-isp/ArduinoISP/#recap-burn-the-bootloader-in-8-steps). Resetting fuses requires 12V programming with a [more convoluted setup](https://www.instructables.com/HV-Rescue-Simple/) and is totally impractical for an Arduino Nano with a soldered SMD microcontroller.

## Checking the logs

Once the uploaded program runs, it produces log statements over the serial interface. You can check these by opening the serial monitor of the Arduino IDE (far right button on the taskbar). On powering up the PCB, the GPS-module needs about half a minute to lock on one or more satellites and after that the LCD shutter program needs 10 GPS pulses before generating the second markers in the driver output towards the LCD shutter. During that time the serial monitor only shows:

```log
    Configuration of LCD shutter completed.
    Stabilizing...
```
After this, the serial monitor shows a few log lines related to the preliminary calibration of the CPU clock frequency relative to the Pulse Per Second (PPS) signal from the GPS module. These lines have the following format:

```log
    Micros: 10 10014560
    CPU: 16023260
    Block: 7822 ticks
```
The CPU frequency should be 16 +/- 0.08 MHz (large error margins because of the cheap ceramic resonator used on the Arduino module). The CPU frequency should only show minor variations during operation. The block ticks refer to the number of required timer events during one half wave (block pulse) of the driver output. This number is derived from the measured CPU frequency and should also show only minor variations.

The calibration, including logging, is repeated every 60 seconds to account for changes in the operating conditions of the Arduino (power voltage, temperature).

In addition to the calibration every minute, the serial monitor shows the phase stability of the driver pulse train, relative to the GPS PPS signal. The log lines look something like the text below, every second:

```log
    LCD phase: 0 277 9 1684
    LCD phase: 0 3 0 12
    LCD phase: 0 4 0 12
```

These are variables used in synchronizing the timer outputs derived from the MCU clock with the GPS PPS signal. The first and third number should be zero and the second number should be small, indicating that synchronization takes place immediately after the incoming GPS pulse. The difference between the fourth and the second number is the accumulated phase differerence during one second in "ticks" of 4 microseconds, just before synchronization happens. This should be a figure between 0 and 32 ticks (meaning between 0 and 128 microseconds). Only the very first log line shows a large phase difference, because the driver pulse train was not synchronized to the GPS signal before that time.

The logging described above is the logging during normal operation. There are two additional log messages that can occur and they indicate possible failures in the system:

1. "Unexpected GPS pulse arrival. Deviation: 21432 microseconds". This indicates instable operation of the GPS module. This can have all kinds of causes, including a too weak GPS signal, an instable power supply, interference from other systems, hardware failure, etc.
2. "Avoidance triggered". This indicates that synchronization starts before the pulse train of the previous second has finished. Possible causes are a bug in the Arduino script and a hardware failure of the Arduino module.

## Bootloader burning on Arduino

Cloned Arduino Nano modules ordered from China may have the so-called "Old bootloader". Although the Arduino IDE offers the option to upload scripts to modules with the "Old bootloader" instead of the default boatloader, this is annoying from a maintenance perspective. The Arduino bootloader burning procedure described [here](https://docs.arduino.cc/built-in-examples/arduino-isp/ArduinoISP/#recap-burn-the-bootloader-in-8-steps) has clear instructions for how to use the Arduino IDE for replacing the bootloader using a second Arduino module, but the wiring instructions are incomplete. Below, a description is added of an Arduino module's ISCP pins as well as their orientation.

### ISCP pins ([source](https://docs.arduino.cc/resources/pinouts/A000066-full-pinout.pdf))

1. CIPO <--> CIPO/D12
2. +5V <--> +5V for Uno and Nano, some boards need +3V
3. SCK <--> SCK/D13
4. COPI <--> COPI/D11
5. RST <--- D10
6. GND <--> GNDS

Learnt from reading several descriptions:

- the RST ISCP pin can only be used as input and is connected to D10 of the programmer board
- other ISCP pins can be used as both input and output
- CIPO connected to CIPO, COPI to COPI (no crosswiring as for serial RX/TX)
- older Arduino descriptions call the ISP pins MISO/MOSI, newer ones CIPO/COPI (controller/peripheral vs master/slave)

### ISCP Header orientation

Arduino modules may have the ISCP connector marked with a `1`, an `ISCP` mark, or both. Pin numbers with respect to these marks go like:

|| | | | | |
||-|-|-|-|-|
|<td colspan=2>mark</td>|
||6|5|&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|1|2|
||4|3| |3|4|
||2|1| |5|6|
||||<td colspan=2>mark</td>|

- my Arduino Nano clone has a mark "1" with pin 1 facing the edge of the board
- official Arduino Uno R3 has a mark "ISCP" with pin 1 facing the center of the board
- marking of clones may differ from legitimate boards

