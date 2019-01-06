#ifndef __CONFIG_H__
#define __CONFIG_H__

/*******************************************************************************
 *
 *  Configuration
 *
 ******************************************************************************/

/* ---- PWM measurement ---- */

/** Debug logging for PWM measurment */
//#define DEBUG_PWM_IN


/** Define if we want just the duty cycle (do not measure frequency, etc.) and save some bits */
#define DUTY_CYCLE_ONLY

/** Define if we want to mesure negative pulse duty cycle */
#define PWM_NEG_MEASURE
// Proliant G8 uses negative PWM

/** How many times we sample (to filter out any noise) pin if we could not detect pulses. Should be an even number. */
#define PWM_IN_RESAMPLE 100


/* ---- EEPROM config store ---- */
//#define DEBUG_EEPROM_CONFIG



/* ---- Ouput PWM signal generation ---- */

/** Debug logging for PWM out */
//#define DEBUG_PWM_OUT

/** Define if you want to invert PWM out */
#define PWM_OUT_NEG

// Proliant G8 uses negative PWM.
// BUT if you choose to add ouput transistor (NPN open collector) then it would invert the signal
// and gets us back to positive PWM out from the Arduino.


/* ---- Command interface ---- */
/** Debug logging command parsing/execution */
//#define DEBUG_CMD_PROC


/* ---- Generic configuration ---- */

/** Program version (also reported via the serial protocol */
#define VERSION      "1.0-RC2"

/** Number of fans outputs. Currently we support at most 2 independent ouputs (limitation by the Arduino. If you
    you have to connect some of them in parallel. */ 
#define FANS         2

/** Number of external temperature sensors connected to analog inputs. Temp 0 is AVR internal temp (always present), Ext. sensors are on A0, A1, ... */
#define TEMP_EXT_SENSORS 3

/* Temperature configuration data (see the description in readme. All the temperatures are in Celsius (for now) */

/** Temp. mapping table step size */
#define TEMP_STEP    5

/** Temp. minimal value. Also any temp. below will be considered as TEMP_MIN */
#define TEMP_MIN     20

/** Temp. max value. Also any temp. above will be considered as TEMP_MAX */
#define TEMP_MAX     55

/** PWM mapping table step size (in percents) */
#define PWM_STEP     5


/* ---- Starting defaults (when EEPROM config is not usable) ---- */

/** Default value for PWM mapping table, i.e. PWM duty cycle for any input */
#define PWM_MAPPING_TABLE_DEFAULT  35

/** PWM in exp. filter - starting value */
#define PWM_EXPFILT_INIT 0.0

/** PWM in exp. filter - starting value */
#define PWM_EXPFILT_WEIGHT 0.05


/** TEMP in exp. filter - starting value */
#define TEMP_EXPFILT_INIT 20.0

/** TEMP in exp. filter - starting value */
#define TEMP_EXPFILT_WEIGHT 0.05


/* ---- Main loop ---- */

//#define DEBUG_LOOP

//#define DEBUG_DATA_PROCESSING

/** How often we send reports. Number of loop iterations */
#define REPORT_FREQ  70

/** Do not send periodic reports (for testing only) */
//#define NO_REPORTS

/** On which pin we have LED for heartbeat (Nano uses D13) */
#define LED_PIN  13


#endif // __CONFIG_H__
