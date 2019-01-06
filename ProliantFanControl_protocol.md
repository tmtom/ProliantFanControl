# ProliantFanControl serial protocol

This is a simple text protocol. It is line based - one line is a single command / response. Line ending is single `\n` character.

# Overview

## Requests

Commands/requests send to the controller have format (very simplified and incomplete):
```
<command_line> = <command> { <delim> <arg> } <eol>
<command>      = "Ver" | "GetCfg" | ...
<delim>        = " " { " " }
<eol>          = '\n'
```

### Checksums

**TODO** - not yet implemented.

## Responses to requests

Responses are more ad-hoc and depend on given command/context. There is always a response. Response is a single line. Possible types of responses:

- `Ok` - Success, no data returned (e.g. when setting something, like `ModeFailsafe`). 
- `E <error report>` - failure. The `<error report>` should further explain what went wrong.
- `<data>` - success and data are returned back (e.g. for some query command like `Ver`).

## Asynchronous messages

There are also asynchronous/unsolicited message sent back from from the controller. All these messages start with `*`. These are for example periodic runtime reports or startup messages.

# Commands

## Get version

Get controller version. Also useful as basic communication test.
Request: `Ver`
Response: `ProliantFanControl 1.0`

## Get HW configuration

Get HW configuration like number of PWM inputs, temperature sensors, fans etc so that the client knows how many configuration parameters there are.
Request: `GetCfg`
Response: `Fans:2 Temps:2 PWM_step:5 PWM_coeffs:21 Temp_min:20 Temp_step:5 Temp_max:80 Temp_coeffs:11`

## Temperature measurement

### Get temperature weights

Get temperature weights for the given fan.
Request: `GetTempWeights F1`
Response: `F1 0.2 0.3 0.5`

### Set temperature weights

Set temperature weights for the given fan. There must be correct number of weights (equal to the number of temperature sensors).
Request: `SetTempWeights F1 0.2 0.3 0.5`
Response: `OK`
Note that if the command failes the values are undefined and you should not issue `SaveTempWeights`.

### Save temperature weights to EEPROM

Save current temperature weights for the given fan to EEPROM. Use it only after successful `SaveTempWeights`.
Request: `SaveTempWeights F1`
Response: `OK`

### Get raw internal temp

**TODO** - not yet implemented.
Read raw A/D data from the internal temperature sensor. Used for callibration at known temperature(s).
Request: `GetRawIntTemp`
Response: `654`

### Get internal temp callibration

**TODO** - not yet implemented.
Get internal temperature callibration  - *offset* and *coefficient*. The formula how they are used is:
realTemp = (rawTemp - *offset* ) / *coefficeint*

Request: `GetIntTempCal`
Response: `324.31 1.22`

### Set internal temp callibration

**TODO** - not yet implemented.
Set internal temperature callibration  - *offset* and *coefficient*.
Request: `SetIntTempCal 324.31 1.22`
Response: `OK`


### Save internal temp callibration

**TODO** - not yet implemented.
Save previously set internal temperature callibration to EEPROM.
Request: `SaveIntTempCal`
Response: `OK`


## Exp filter

### Get exp filtering coeff

Exponential filter weights for PWM and temperature measurements.
Default 0.1
Request: `GetPwmFilt`
Response: `0.2 0.1`

### Set PWM filtering coeff

Request: `SetPwmFilt 0.2 0.1`
Response: `OK`

### Save PWM filtering coeff

Request: `SavePwmFilt`
Response: `OK`

## Mapping table

### Get mapping table

Get part of the PWM mapping table for the given fan and temperature.
Request: `GetPwmMap F1 T:20`
Response: `F1 T:20 0 10 15 20 25 30 35 40 45 50 55 60 65 70 75 80 85 90 95 100`

### Set mapping table

Request: `SetPwmMap F1 T:20 0 10 15 20 25 30 35 40 45 50 55 60 65 70 75 80 85 90 95 100`
Response: `OK`

### Save mapping table to EEPROM

Request: `SavePwmMap F1 T:20`
Response: `OK`

## Operational mode

### Switch to manual mode

Request: `ModeManual F1:20 F2:30`
Response: `OK`

### Switch to auto mode

Request: `ModeAuto`
Response: `OK`

### Switch to failsafe mode

Request: `ModeFailsafe`
Response: `OK`

## Asynchronous reports

All asynchronous reports start with `*` as the first charater on the line to distinguish asynchronous reports from standard command responses.

### Autonomous mode

`*A PWM1_in:20 T1_in:23 T2_in:30 F1_out:20 F2_out:30`

### Manual mode

`*M PWM1_in:20 T1_in:23 T2_in:30 F1_out:20 F2_out:30`

### Failsafe mode (copy input)

`*F PWM1_in:20 T1_in:23 T2_in:30 F1_out:20 F2_out:30`

### Runtime errors

Some examples:

`*E Manual mode timeout, switching to failsafe...`

`*E EEPROM checksum mismatch (...). Using failsafe mode.`
