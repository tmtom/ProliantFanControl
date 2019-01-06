#ifndef __DATAPROCESSING_H__
#define __DATAPROCESSING_H__


/** 
 * Filter new measured value
 * 
 * @param oldVal old filter value (will be updated)
 * @param weight filter weight
 * @param newVal newly measured value
 * 
 * @return new filtered value
 */
float ExpFilter(float *oldVal, float weight, float newVal);


/** 
 * Average measured temperatures for given fan
 * 
 * Using global temps[] as input, compute wighted average and do saturation against temp limits.
 *
 * @param fan fan index (0 .. NUM_FANS-1)
 * 
 * @return resulting temperature
 */
int averageTemp(unsigned char fan);


/** 
 * Bilinear interpolation in the mapping table using PWM and temperature as inputs
 * 
 * @param fan zero based fan index (0 .. NUM_FANS-1)
 * @param pwm input PWM value (0 .. 100)
 * @param tmp input tempetarture index (already averaged/wighted, sanitized, etc.)
 * 
 * @return resulting PWM (0 .. 100)
 */
int interpolatePwm(unsigned char fan, unsigned char pwm, unsigned char tmp);


#endif // __DATAPROCESSING_H__
