#ifndef __PWMOUT_H__
#define __PWMOUT_H__

/*******************************************************************************
 *
 *  PWM out
 *
 ******************************************************************************/

/* -----------------------------------------------------------------------
   PWM out 1 - using timer 0 (cannot use delay), controlling a fan on D5
   ----------------------------------------------------------------------- */

/** 
 * Start the PWM using timer 0, ouput on D3
 * 
 * @param duty see @pwmSetDcA
 */
void pwmBeginA(unsigned int duty);


/** 
 * Set duty cycle
 * 
 * @param duty desired fan duty cycle in percents (0-100)
 */
void pwmSetDcA(unsigned int duty);


/* -----------------------------------------------------------------------
   PWM out 2 - using timer2, controlling a fan on D3
   ----------------------------------------------------------------------- */

/** 
 * Start the PWM using timer 2, ouput on D3
 * 
 * @param duty see @pwmSetDcB
 */
void pwmBeginB(unsigned int duty);


/** 
 * Set duty cycle
 * 
 * @param duty desired fan duty cycle in percents (0-100)
 */
void pwmSetDcB(unsigned int duty);


#endif // __PWMOUT_H__
