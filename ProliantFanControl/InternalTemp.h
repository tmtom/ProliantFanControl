#ifndef __INTERNALTEMP_H__
#define __INTERNALTEMP_H__

/*******************************************************************************
 *
 *  Filtering - exponential filter used for temperature
 *
 ******************************************************************************/
extern float intTempExpFilterVal;  /**< Current filter value */

/*******************************************************************************
 *
 *  Internal temperature measurements
 *
 ******************************************************************************/

// Datasheet says about 1mV/C, example in the DS is more like 1.1 and offset 288
// See also:
//  - http://ww1.microchip.com/downloads/en/AppNotes/Atmel-8108-Calibration-of-the-AVRs-Internal-Temperature-Reference_ApplicationNote_AVR122.pdf
//  - http://www.avdweb.nl/arduino/hardware-interfacing/temperature-measurement.html
//  - http://www.netquote.it/nqmain/2011/04/arduino-nano-v3-internal-temperature-sensor/
//  - http://nerdralph.blogspot.com/2014/08/writing-library-for-internal.html <<==
//
// These values are from the Arduino web. Either way, the sensor needs calibration and some filtering.
#define AVR_INT_TEMP_offset 324.31
#define AVR_INT_TEMP_coeff    1.22


/** 
 * Read AVR internal temperature
 *
 * Requires calibration.... 
 * 
 * @return internal temperature in C
 */
float readInternalTemp(void);


#endif // __INTERNALTEMP_H__
