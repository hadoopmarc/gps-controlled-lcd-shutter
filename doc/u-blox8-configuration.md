# U-blox8 configuration

## Setup
- RPI400 with Raspbian 11 (bullseye)
- pygpsclient-1.4.6 on python-3.9.2
- USB/UART programmer
- U-blox NEO-M8T module

The pygpsclient is only used for setting the ubx configuration variables, but is also useful to check the satellite fix on a specific location. Note that on an RPI400 the client cannot keep up with showing NMEA messages in the console and the displayed UTC time starts lagging. This does not occur when the console is disabled in the view settings.

## Available configuration examples
The [U-blox8 configuration guide](https://content.u-blox.com/sites/default/files/products/documents/u-blox8-M8_ReceiverDescrProtSpec_UBX-13003221.pdf) gives the following examples for configuring the TIMEPULSE/TIMEPULSE2 outputs of the NEO-M8T module with the ubx protocol (section 19.5, page 74):

| ubx key           | ubx value 1 | ubx value 2 | comment |
|: ------------     | ----------- | ----------- |: ------ |
| tpIdx             | 0           | 1           | TIMEPULSE vs TIMEPULSE2
| freqPeriod        | 1 s         | 1 Hz        | unit depends on isFreq
| pulseLenRatio     | 100 ms      | 0           | unit depends on isLength
| freqPeriodLock    |             | 10 MHz      | depends on lockedOtherSet
| pulseLenRatioLock |             | 50%         | depends on lockedOtherSet
| active            | 1           | 1           | configuration is active
| lockGnssFreq      | 1           | 1           | most precise but probably irrelevant
| lockedOtherSet    | 0           | 1           | for recognizing no-lock situation
| isFreq            | 0           | 1           | unit for freqPeriod(Lock)
| isLength          | 1           | 0           | unit for pulsLenRatio(Lock)
| alignToTow        | 1           | 1           | most precise but probably irrelevant
| polarity          | 1           | 1           | 1 = rising edge
| gridUtcGnss       | 1           | 1           | should be 0 (UTC) for astronomy

The lockedOtherSet allows a no-lock situation to be made visible through the frequency and pulse length ratio of the timepulse. This seems useful for the LCD application.

## Configuration for LCD shutter

| ubx key           | TIMEPULSE   | TIMEPULSE2  | comment |
|: ------------     | ----------- | ----------- |: ------ |
| tpIdx             | 0           | 1           | TIMEPULSE vs TIMEPULSE2
| freqPeriod        | 1 Hz        | 16 Hz       | unit depends on isFreq
| pulseLenRatio     | 50000       | 15625       | unit depends on isLength
| freqPeriodLock    | 1 Hz        | 16 Hz       | depends on lockedOtherSet
| pulseLenRatioLock | 50000       | 31250       | depends on lockedOtherSet
| active            | 1           | 1           | configuration is active
| lockGnssFreq      | 1           | 1           | most precise but probably irrelevant
| lockedOtherSet    | 0           | 1           | for recognizing no-lock situation
| isFreq            | 1           | 1           | unit for freqPeriod(Lock)
| isLength          | 1           | 1           | unit for pulsLenRatio(Lock)
| alignToTow        | 1           | 1           | most precise but probably irrelevant
| polarity          | 1           | 1           | 1 = rising edge
| gridUtcGnss       | 0           | 0           | should be 0 (UTC) for astronomy

Other fields keep their default value.
isLength = 0 does not seem to work, so rather pulse duration in microseconds are provided.




