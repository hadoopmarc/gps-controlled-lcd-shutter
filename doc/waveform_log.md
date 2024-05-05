# Validation using the waveform log

A typical log of the waveform sketch is listed farther down the file. It shows:

- the sketch needs 11 seconds for stabilizing before outputting LCD phase measurements

- it needs another 10 GPS pulses before it calibrates the CPU frequency from 16.000 MHz to 16.013 MHz and shifts to a different phase control pattern

To control the syncing of the LCD driving pattern with the GPS pulse, the sketch expresses the LCD phase in two ways:

1. phase = (iHalfWave * OCR1A + TCNT1) * TICK_MICROS, with OCR1A the number of timer ticks between timer interrupts (32 times per second), TCNT1 the number of timer ticks since the last interrupt and TICK_MICROS a value of 4 CPU clock microseconds

2. phase = observedDiff = t_s - lastGpsMicros, with t_s the current non-critical time of processing just before syncing the LCD driving pattern with the GPS pulse and lastGpsMicros the earlier measured time immediately after receiving a GPS pulse, both in CPU clock microseconds

The LCD phase measurements logged are:

- iIsr: for monitoring/debugging only. It is reset to zero directly after receiving a GPS pulse and incremented by one after each timer interrupt. Logging should show iIsr = 0 after stabilizing.
- observedTicks: observedDiff / TICK_MICROS. Logging shows a small value (< 10), when the Arduino only controls the LCD-shutter.
- oldHalfWave: the iHalfWave value just before the sync update. Logging should show iHalfWave = 0 after stabilizing.
- oldTCNT1: the TCNT1 value in TICK_MICROS just before the sync update. In perfect sync oldTCNT1 - observedTicks should be very small. However, the applied TIMER_SAFETY = 2 adds 2 * N_HALF_WAVE = 64 ticks to the ideal phase difference.

Before CPU calibration, a slower CPU frequency of 16.000 MHz is assumed instead of the actual 16.013 MHz. As result, the LCD driving pattern started at the previous GPS pulse, has already finished when the new GPS pulse arrives. This is consistent with the values around 14:41:27.207:

- iIsr = 0
- observedTicks = 3
- oldHalfWave = 0
- oldTCNT1: 232

So, in a period of 1 seconds, the LCD driving pattern gets ahead of the GPS pulse by (232 - 3) * TICK_MICROS = 916 microseconds. This is slightly more than expected from the end-to-end time difference measurement for CPU calibration: 20016004 in 20 seconds amounts to 800 micros per second. This has to do with the rounding required for OCR1A and the TIMER_SAFETY = 2 applied (2 * TICK_MICROS * N_HALF_WAVE = 256 microseconds).

After calibration of the CPU frequency at 14:41:34.249 logged LCD phase parameters remain stable and within specification.

14:41:13.634 -> Configuration of LCD shutter completed  
14:41:13.681 -> Stabilizing...  
14:41:25.185 -> LCD phase: 0 4 17 4899  
14:41:26.215 -> LCD phase: 0 3 0 231  
14:41:27.207 -> LCD phase: 0 3 0 232  
14:41:28.199 -> LCD phase: 0 3 0 232  
14:41:29.187 -> LCD phase: 0 3 0 232  
14:41:30.181 -> LCD phase: 0 3 0 232  
14:41:31.201 -> LCD phase: 0 3 0 232  
14:41:32.183 -> LCD phase: 0 3 0 232  
14:41:33.217 -> LCD phase: 0 3 0 232  
14:41:34.203 -> LCD phase: 0 3 0 232  
14:41:34.203 -> Micros: 20 20016004  
14:41:34.249 -> CPU: 16012802  
14:41:34.249 -> Block: 7816 ticks  
14:41:35.190 -> LCD phase: 0 3 0 39  
14:41:36.173 -> LCD phase: 0 3 0 40  
14:41:37.204 -> LCD phase: 0 3 0 39  
14:41:38.190 -> LCD phase: 0 3 0 39  
14:41:39.218 -> LCD phase: 0 3 0 39  
14:41:40.205 -> LCD phase: 0 3 0 39  
14:41:41.177 -> LCD phase: 0 3 0 39  
14:41:42.200 -> LCD phase: 0 3 0 39  
14:41:43.173 -> LCD phase: 0 3 0 39  
14:41:44.204 -> LCD phase: 0 3 0 39  
14:41:45.193 -> LCD phase: 0 4 0 39  
14:41:46.175 -> LCD phase: 0 3 0 40  
14:41:47.203 -> LCD phase: 0 3 0 39  
14:41:48.183 -> LCD phase: 0 3 0 39  
14:41:49.212 -> LCD phase: 0 4 0 39  
14:41:50.197 -> LCD phase: 0 3 0 40  
14:41:51.182 -> LCD phase: 0 3 0 39  
14:41:52.214 -> LCD phase: 0 3 0 38  
14:41:53.202 -> LCD phase: 0 3 0 39  
14:41:54.185 -> LCD phase: 0 3 0 39  
14:41:54.232 -> Micros: 40 40032004  
14:41:54.232 -> CPU: 16012802  
14:41:54.279 -> Block: 7816 ticks  
14:41:55.220 -> LCD phase: 0 3 0 38  
14:41:56.206 -> LCD phase: 0 3 0 39  
14:41:57.188 -> LCD phase: 0 5 0 40  
14:41:58.216 -> LCD phase: 0 3 0 40  
14:41:59.189 -> LCD phase: 0 3 0 39  
14:42:00.175 -> LCD phase: 0 3 0 39  
14:42:01.199 -> LCD phase: 0 3 0 39  
14:42:02.181 -> LCD phase: 0 3 0 39  
14:42:03.212 -> LCD phase: 0 3 0 39  
14:42:04.197 -> LCD phase: 0 3 0 39  
14:42:05.182 -> LCD phase: 0 3 0 39  
14:42:06.217 -> LCD phase: 0 3 0 39  
14:42:07.204 -> LCD phase: 0 3 0 39  
14:42:08.192 -> LCD phase: 0 3 0 39  
14:42:09.178 -> LCD phase: 0 3 0 39  
14:42:10.205 -> LCD phase: 0 3 0 39  
14:42:11.186 -> LCD phase: 0 3 0 39  
14:42:12.213 -> LCD phase: 0 3 0 39  
14:42:13.186 -> LCD phase: 0 3 0 39  
14:42:14.216 -> LCD phase: 0 4 0 39  
14:42:14.216 -> Micros: 60 60048004  
14:42:14.262 -> CPU: 16012801  
14:42:14.262 -> Block: 7816 ticks  
14:42:15.205 -> LCD phase: 0 3 0 40  
14:42:16.198 -> LCD phase: 0 4 0 39  
14:42:17.185 -> LCD phase: 0 3 0 40  
14:42:18.175 -> LCD phase: 0 4 0 39  
14:42:19.212 -> LCD phase: 0 3 0 40  
14:42:20.206 -> LCD phase: 0 3 0 39  
14:42:21.184 -> LCD phase: 0 3 0 39  
14:42:22.217 -> LCD phase: 0 3 0 39  
