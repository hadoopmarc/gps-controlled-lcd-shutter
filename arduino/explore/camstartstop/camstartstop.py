import serial  # python package pyserial

# To run in WSL2, first run in windows shell:
# winget install --interactive --exact dorssel.usbipd-win
# usbipd list
# usbipd bind --busid 1-2
# usbipd attach --wsl --busid 1-2

ser = serial.Serial('/dev/ttyUSB0', 115200)
print(ser.read(30).decode())
# (voltage) marc:camstartstop$ python camstartstop.py
# 509860471
# <camstart>
# 7131611
# (voltage) marc:camstartstop$
