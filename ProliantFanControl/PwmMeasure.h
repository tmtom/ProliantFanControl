#ifndef __PWMMEASURE_H__
#define __PWMMEASURE_H__

#include "Config.h"


/*******************************************************************************
 *
 *  Filtering - exponential filter used for PWM duty cycle
 *
 ******************************************************************************/

extern float pwmExpFilterVal;     /**< Current filter value. */
extern float pwmExpFilterWeight;  /**< Weight for the exponential filter, Let's make it "slower".  */


/*******************************************************************************
 *
 *  PWM measurment
 *
 * Taken from https://forum.arduino.cc/index.php?topic=410853.msg2833766#msg2833766
 * plus some refactoring/modifications...
 * Another, more generic solution is in http://forum.arduino.cc/index.php?topic=413133.msg2844242#msg2844242
 *
 * So far seems to be OK, but there are some ugly "heuristic" filters just based on my system.
 * This should be more robust and generic (see pwmMeasureComplete) ...
 ******************************************************************************/


#ifndef DUTY_CYCLE_ONLY
extern float pwmPeriod;    /**< Output - PWM period in us */
extern float pwmFrequency; /**< Output - PWM frequency in kHz */
extern float pwmPWidth;    /**< Output - pusitive pulse width in us */
#endif
extern float pwmDuty;      /**< Ouptut - PWM duty cycle */


/** 
 * Start PWM duty cycle measurement
 * 
 * Check the measurement end with @mmmm.
 * Be careful with using other interrupts during the measurment
 */
void pwmMeasureBegin (void);


/** 
 * Timer1 capture handler
 *
 * Timer1 has been copied to ICR1 on the given edge.
 */
ISR (TIMER1_CAPT_vect);


/** 
 * Timer1 overflow handler
 * 
 * Happens if there is no edge within 65536 * 0.625us = 4096us ~ 244Hz
 * I.e. if we have 0% or 100% duty cycle.
 */
ISR (TIMER1_OVF_vect);


/** 
 * Finish measurements
 * 
 * Once finished the measured values (based on the config) are in:
 *  - pwmPeriod    - PWM period in us
 *  - pwmFrequency - PWM frequency in kHz
 *  - pwmPWidth    - pusitive pulse width in us
 *  - pwmDuty      - PWM duty cycle
 *
 * @return 0 when when complete and fills the output
 */
int pwmMeasureComplete (void);


#endif // __PWMMEASURE_H__

