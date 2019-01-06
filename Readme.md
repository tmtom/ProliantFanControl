
# ProliantFanControl

# Disclaimer

No warranty, use at your own risk ....

# Introduction

https://homeservershow.com/forums/topic/11253-%E2%80%8B-hp-microserver-gen-8-fan-speed-all-you-need-to-know/
https://homeservershow.com/forums/topic/7294-faking-the-fan-signal/?/topic/7294-faking-the-fan-signal/?p=79985
http://www.silentpcreview.com/article1377-page9.html




# HW design

Use Arduino (Nano), intercept PWM fan control signal(s), measure temperature and create new PWM output for fan(s).

Connect Arduino to the internal USB port so that we can (if we want to):

* Control and configure it in runtime

* Update its firmware. No need to re-open the server (see final comments below)

This is optional. Once setup and configured we do not have to use any of this and leave it in its "autonomous mode".

Or we can ignore the logic of the controller and use it to directly control the fans by some application ourselves.

# SW high level design

The goal is to take original PWM signal (ideally from every fan), possibly add other inputs like temperature measurement and based on these compute output PWM signal for each fan.

Chosen Arduino Nano as it is fairly cheap and should have enough resources for the task. With it (Arduino Nano, Atmel ATmega328) we can measure PWM in 2 ways:

* Direct measurement of PWM timing - no extra HW required, can be very accurate but we can (easilly) measure only one signal (using timer1). In the next generation we could switch to an STM32 board and have more inputs.
  
* Using low pass filter convert PWM to analog voltage and use ADC which is multiplexed and we could measure several signals. But this requires extra HW and possibly some calibration.

Chosen the first option, thought that single PWM input should be enough espcially if we select the right one (probably the fan closest to the CPU(s)).

If we want we can easilly add few extra temperature sensors as well (plus the Arduino can measure its temperature as well). The readings will be consolidated for each fan to a single value using a weighted average (user configurable wights for each fan).

To be flexible we have chosen for the conversion of input data to output simple mapping table which is user configurable. To save space the table contains values only at given raster/step and we use bilinear interpolation for the values in between.

With the Arduino we can have on the ouput 2 PWM signals (timer0 and timer1) since we need to adjust specific PWM frequency (25 kHz).

The whole algorithm is then:

* Measure PWM duty cycle from the computer
  
* Measure all the temperatures
  
* For each ouput fan signal (currently 2):
  
  * Compute single temperature from all the sensors using weighted average with wights for given fan output
  * Using mapping table for the given fan output lookup output value for the given PWM input and temperature
  * If it is in between the raster/steps use bilinear interpolation using the 4 neighbors
  * Set the PWM out duty cycle to the value

This is also illustrated on the following block diagram:

```text
                                      +-----------------+
Fan PWM in -------------------------->| Mapping table   |-+
                                      |  for Fan 1      | |---------> PWM out Fan1
                 +-----------+        | (+ bilinear     | |---------> PWM out Fan2
Temperature 1 -->| Weighted  |-+ ---->|  interpolation) | |---------> ...
Temperature 2 -->| average   | |      +-----------------+ |
...           -->| for Fan 1 | |        +-----------------+
                 +-----------+ |
                   +-----------+
```

There is also some averaging/filetering to remove noise and avoid sudden changes.

## Configuration

The controler is user configurable. The configuration consists of:

* For each fan

  * Temperature sensor weights (float)

  * Two dimensial mapping table from input PWM and temperature to the new PWM (in the given "raster")

* Coefficient of the input PWM exponential filter

User can read/write/save these configuration parameters using serial protocol (see another document). For details see the protocol description.

Number of fans, temperature sensors, raster of the mapping table is configurable in the source code only. Once compiled and flashed the it is fixed.

## Operational modes

### Autonomous mode

Default one - the algorithm described above.

### Manual mode

Direct control by the user of the fans.

### Failsafe mode

"Copy" PWM input to ouptut

## Communication

Protocol - see the protocol doc.

In the future we should consider simple checksum to protect from random port probing by other apps / system.

The Arduino be reprogrammed while in the server (if it is fast enough so that system does not complain about fan malfunction). At the moment disabled autorestart to speedup startup (so cannot get to the bootloader without manually pressing the reset button).




