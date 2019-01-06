#include <Arduino.h>
#include "Config.h"
#include "DataProcessing.h"
#include "PwmMeasure.h"

float pwmExpFilterVal     = PWM_EXPFILT_INIT;
float pwmExpFilterWeight  = PWM_EXPFILT_WEIGHT;

volatile byte tmr1State = 0;    /**< Internal measurement phase/state. Starts at 0, at 5 is complete.*/
volatile word tmr1Value[4];     /**< Internal counts - see the @ISR(TIMER1_CAPT_vect) */


#ifndef DUTY_CYCLE_ONLY
float pwmPeriod    = 0.0f;      /**< Output - PWM period in us */
float pwmFrequency = 0.0f;      /**< Output - PWM frequency in kHz */
float pwmPWidth    = 0.0f;      /**< Output - pusitive pulse width in us */
#endif
float pwmDuty      = 0.0f;      /**< Ouptut - PWM duty cycle */


void pwmMeasureBegin (void)
{
    TCCR1A    = 0;                         // normal operation mode
    TCCR1B    = 0;                         // stop timer clock (no clock source)
    TCNT1     = 0;                         // clear counter
    TIFR1     = bit (ICF1)  | bit (TOV1);  // clear flags
    tmr1State = 0;                         // clear tmr1State
    TIMSK1    = bit (ICIE1) | bit (TOIE1); // interrupt on input capture for measurement and overflow to handle "flatline" of 0% and 100%
    TCCR1B    = bit (CS10)  | bit (ICES1); // start clock with no prescaler, rising edge on pin D8
}


ISR (TIMER1_CAPT_vect)
{
    switch (tmr1State) {

    case 0:                          // first rising edge
        tmr1Value[0] = ICR1;
        tmr1State = 1;
        break;

    case 1:                          // second rising edge
        tmr1Value[1] = ICR1;
        TIFR1       |=  bit (ICF1);  // after edge change the we should clear the Input Capture Flag
        TCCR1B      &= ~bit (ICES1); // capture on falling edge (pin D8)
        tmr1State    = 2;
        break;

    case 2:                          // first falling edge
        tmr1State = 3;
        break;

    case 3:                          // second falling edge
        tmr1Value[2] = ICR1;
        tmr1State    = 4;
        break;

    case 4:                          // third falling edge
        tmr1Value[3] = ICR1;
        tmr1State    = 5;            // all tests done
        TIMSK1       = 0;            // stop timer1 interrupts
        break;

    default:
        break;
    }
}


/** 
 * Simple average pin sampler for PWM input
 * 
 * @return  average pin value
 */
int ResamplePwmPin(void)
{
    unsigned int sum = 0;
    for(int i=0; i<PWM_IN_RESAMPLE; ++i)
    {
        if(digitalRead(8))
            ++sum;
        __builtin_avr_delay_cycles(1);
    }   

    if(sum < (PWM_IN_RESAMPLE/2))
        return 0;

    return 1;
}


ISR (TIMER1_OVF_vect)
{
    // Just check the pin and mark measurement as completed. Using "magic combinations" to indicate that..
    if(ResamplePwmPin()) {
        tmr1Value[0] = 100;
        tmr1Value[1] = 100;
        tmr1Value[2] = 100;
        tmr1Value[3] = 100;
    } else {
        tmr1Value[0] = 0;
        tmr1Value[1] = 0;
        tmr1Value[2] = 0;
        tmr1Value[3] = 0;
    }
    
    tmr1State = 5;                 // all tests done
    TIMSK1    = 0;                 // stop the ints
}


int pwmMeasureComplete (void)
{
    if(tmr1State != 5)
        return 1;

    // timer1 ints (TIMSK1) should be already disabled after the measurement
    // is complete and other ints should not bother us.

#ifdef DEBUG_PWM_IN
    for(int i=0; i<4; ++i) {
        Serial.print(i);
        Serial.print(":\t");
        Serial.println(tmr1Value[i]);
    }
#endif

    // no pulses - 0% or 100%
    if( tmr1Value[0]==tmr1Value[1] && tmr1Value[1]==tmr1Value[2] && tmr1Value[2]==tmr1Value[3])
    {
#ifdef PWM_NEG_MEASURE
        if(tmr1Value[0] == 0)
            pwmDuty = 100.0f;
        else
            pwmDuty = 0.0f;
#else
        if(tmr1Value[0] == 0)
            pwmDuty = 100.0f;
        else
            pwmDuty = 0.0f;
#endif

#ifndef DUTY_CYCLE_ONLY
        pwmPeriod    = 0.0f;
        pwmPWidth    = 0.0f;
        pwmFrequency = 0.0f;
#endif
    }
    else
    {
        // Period = 3rd neg edge - 2nd neg edge. The same result should be also tmr1Value[1] - tmr1Value[0]
        // i.e. 2nd pos - 1st pos
        word periodValue = tmr1Value[3] - tmr1Value[2]; 

        // TODO - add some filtering.
        // there are some issues with very short negative pulses - sometimes we catch them, sometimes not
        // This helps but not too much... still some work to do
        word periodValue1 = tmr1Value[1] - tmr1Value[0];

        // (3rd neg - 2nd pos) should be 1 period + 1 pos pulse (usually is 2 or 3, why?)
        word widthValue  = tmr1Value[3] - tmr1Value[1];

        // BUT we may have missed something and have several periods + 1 pos pulse, so use modulo
        //word pwmWidth = widthValue - 2*periodValue;
        word pwmWidth = widthValue % periodValue;
        unsigned pers = widthValue / periodValue;

        // Some heuristic filtering
        //   - check for reasonable deviation in period
        //   - and that we have also captured the pulse noy too behind (max 3 periods)
        // If either check fails it means there are just too short pulses/noise??
        // add long time period average check!!!!
        int diff = (int)((long)periodValue - (long)periodValue1);
        if ( (abs(diff) < 50)
             &&
             (0<=pwmWidth && pwmWidth<=periodValue)
             &&
             (pers < 4) )
        {        
#ifdef DEBUG_PWM_IN
            Serial.print("widthValue ");
            Serial.print(widthValue);
            Serial.print(", pers ");
            Serial.println(pers);

            Serial.print("pwmWidth ");
            Serial.print(pwmWidth);
            Serial.print("   periodVal ");
            Serial.println(periodValue);
#endif

#ifndef DUTY_CYCLE_ONLY
            pwmPeriod    = periodValue * 0.0625;
            pwmPWidth    = pwmWidth * 0.0625;
            pwmFrequency = 1000 / pwmPeriod;
#endif

#ifdef PWM_NEG_MEASURE
            pwmDuty = (periodValue - pwmWidth) * 100.0 / periodValue;
#else
            pwmDuty = pwmWidth * 100.0 / periodValue;
#endif
        }
        else
        {
            // typically impuls too short impuls
#ifdef DEBUG_PWM_IN
            Serial.println("Some garbage...");
            Serial.print("Period diff ");
            Serial.print(diff);
            Serial.print(", widthValue ");
            Serial.print(widthValue);
            Serial.print(", pers ");
            Serial.println(pers);
            Serial.print(", pwmWidth ");
            Serial.println(pwmWidth);
#endif

#ifdef PWM_NEG_MEASURE
            if(ResamplePwmPin())
#else
                if(!ResamplePwmPin())
#endif
                    pwmDuty = 0.0;
                else
                    pwmDuty = 100.0;

#ifndef DUTY_CYCLE_ONLY
            pwmPeriod    = 0.0f;
            pwmPWidth    = 0.0f;
            pwmFrequency = 0.0f;
#endif
        }
    }

#ifdef DEBUG_PWM_IN
    Serial.print("Duty cycle: ");
    Serial.println(pwmDuty);
#ifndef DUTY_CYCLE_ONLY
    Serial.print("Period ");
    Serial.print(pwmPeriod);
    Serial.print("us, pulse width ");
    Serial.print(pwmPWidth);
    Serial.print("us, frquency ");
    Serial.print(pwmFrequency);
    Serial.println("kHz");
#endif
#endif


    pwmDuty = ExpFilter(&pwmExpFilterVal, pwmExpFilterWeight, pwmDuty);

    return 0;
}
