# Smart plug management for the ClearSky detector

## Preconfiguration
These are the instructions to configure a smart plug with Tasmota software for use with the ClearSky detector.

When first powering up a smart plug with Tasmota firmware after flashing, upgrading or recovery, the Wi-Fi manager becomes active as a hotspot with an SSID in the format tasmota-ABCDEF-1234. You can connect to it with a laptop or smartphone and then browse to the http://192.168.4.1/ fixed uri (this page might open automatically). Then just fill in the SSID and password of the local Wi-Fi network that the plug should use.

The following configurations are minimally required in the *Other* section of the plug UI's configuration menu:
- Template, this should be Athom V2 or Gosund EP2
- Specify a web admin password for basic authentication on the web UI and store it in your password manager
- Disable MQTT
- Device Name (e.g. Athom Tasmota M2, where "M2" is some marker written on the physical housing for easy recognition, this name appears in the web UI)
- Friendly Name (e.g. tasmota-e012bb-4795, this appears in the "sudo nmap -sP 192.168.2.0/24")

And in the *Module* section:
- Module type, this should be Athom V2 or Gosund EP2
- GPIOmn, leave at None

In the console set the timezone to UTC with the command:

```text
Timezone +0:00
```

In the *Timer* section:
- enable timers
- timer 1: enable, repeat, output 1 off, time 12:00 +/- 00, su until mo
- timer 2: enable, repear, output 1 on,  time 12:01 +/- 00, su until mo

### Tasmota Wi-Fi behaviour
Although the configuration menu in the Tasmota web UI offers an option to enter alternative values for the ssid and password, the firmware does not try and use these alternative values when the primary values fail. This might be a bug (tested on v 12.5.0 on the Athom v2 and Gosund EP2 plugs). Possibly, the secondary access point settings rather apply to a second Wi-Fi module that can be connected to an ESP32 chip (not for ESP8266).

The behaviour described above implies that outside the reach of the configured Wi-Fi network the Wi-Fi settings of a Tasmota smart plug can only be modified via [device recovery](https://tasmota.github.io/docs/Device-Recovery/). However, device recovery is painful because all settings are reset to firmware defaults. Therefore, moving a smartplug from one Wi-Fi network to another requires a mobile hotspot, as described in section [Safe remote deployment](#safe-remote-deployment).

A more user friendly, but less safe workaround during experimentation is to change the [WifiConfig](https://tasmota.github.io/docs/Commands/#wi-fi) from the default *4 = retry other AP without rebooting* to *2 = set Wi-Fi Manager as the current configuration tool*. If the device cannot connect to the Wifi using the primary access point settings, the [WifiManager](https://github.com/tzapu/WiFiManager/blob/master/README.md#how-it-works) is run for 3 minutes during which the smart plug functions as a Wi-Fi access point and runs a web UI on the 192.168.4.1 IP address (as in the case of [first operation](#preconfiguration)). There, it is possible to configure new values for the ssid and password.

One can apply this workaround manually by entering the following commands in the *console* of the smart plug's web UI:

- `WifiConfig 2`
- `Restart 1`

Alternatively, you can add `"CMND":"WifiConfig 2"` to the template mentioned earlier.

As said before, this workaround has a safety issue: if the primary Wi-Fi is unavailable, anyone in the neighbourhood of the plug can detect it functioning as an access point without WPA2 security. The username/password protection is run by - assumably - vulnerable software. Also, the password can be read in case of physical access to the plug.

## Safe remote deployment
These are the instructions to deploy a smart plug for the ClearSky detector on a remote location. Reliable operation is only possible with Wi-Fi, because the smart plug can contact NTP servers but does not have battery backup for the system clock (so, time will be incorrect after losing power).

First be sure to have a mobile hotspot available and then - if needed - change the Wi-Fi settings of the smart plug to the ssid and password of the mobile hotspot.

At the remote location activate the mobile hotspot and the smartplug. Then change the Wi-Fi settings of the smart plug to the ssid and password of the Wi-Fi network of the remote location.