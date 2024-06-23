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

## Checking the logs

Once the uploaded program runs, it produces log statements over the serial interface. You can check these by opening the serial monitor of the Arduino IDE (far right button on the taskbar). On powering up the PCB, the GPS-module needs about half a minute to lock on one or more satellites and after that the LCD shutter program need 10 GPS pulses before generating second markers in the driver output. During that time the serial monitor only shows:

```log
    Configuration of LCD shutter completed.
    Stabilizing...
```

After this time the serial monitor shows something like the text below, every second:

```log
    LCD phase: 0 3 0 12
```

These are variables used in synchronizing the timer outputs derived from MCU clock with the Pulse Per Second signal from the GPS module. The first and third number should be zero and the second number should be small, indicating that synchronization takes place immediately after the incoming GPS pulse. The difference between the fourth and the second number should is the accumulated phase differerence in "ticks" of 4 microseconds during one second, just before synchronization happens. This should be a figure between 0 and 32 ticks (meaning between 0 and 128 microseconds).

In addition to the logs that occur every second, another log message appears every 20 seconds and shows the result of calibrating the MCU clock frequency against the GPS PPS signal. It has the following format:

```log
    Micros: 20 20001234
    CPU: 16012345
    Block: 7810 ticks
```
The CPU frequency should be 16 MHz with an error margin of 0.5% (so large because of the cheap ceramic resonator used on the Arduino module). The CPU frequency should only show minor variations during operation. The block ticks refer to the number of required timer events during one half wave (block pulse) of the driver output. This number is derived from the measured CPU frequency and should also show only minor variations.
