#include <Arduino.h>
#include "Config.h"
#include "PwmOut.h"

// Common macros for PWM out. Duty range is 0-80

#ifdef PWM_OUT_NEG
#define PWM_OUT_0        HIGH
#define PWM_OUT_100      LOW
#define PWM_OUT_VAL(dc)  ((100 - (dc)) * 79 / 100)
#else
#define PWM_OUT_0        LOW
#define PWM_OUT_100      HIGH
#define PWM_OUT_VAL(dc)  ((dc) * 79 / 100)
#endif

/* -----------------------------------------------------------------------
   PWM out 1 - using timer 0 (cannot use delay), controlling a fan on D5
   ----------------------------------------------------------------------- */

unsigned char pwmA_Dc = 0; /**< Is PWM B just DC mode (0 or 100%)? */


void pwmBeginA(unsigned int duty)
{
    pwmA_Dc = 0;
    TCCR0A  = 0;                              // TC2 Control Register A
    TCCR0B  = 0;                              // TC2 Control Register B
    TIMSK0  = 0;                              // TC2 Interrupt Mask Register
    TIFR0   = 0;                              // TC2 Interrupt Flag Register
    TCCR0A |= bit(COM2B1) | bit(WGM21) | bit(WGM20);  // OC2B cleared/set on match when up/down counting, fast PWM
    TCCR0B |= bit(WGM22) | bit(CS21);         // use clock/8, ie. res 0.5us
    OCR0A   = 79;                             // TOP overflow value is 80 producing PWM 80 * 0.5us = 40us = 25kHz
    pwmSetDcA(duty);                             
    pinMode(5, OUTPUT);
}


void pwmSetDcA(unsigned int duty)
{
    // Handle DC 0 and 100 as special cases
    if(duty==0)
    {
        pwmA_Dc = 1;
#ifdef DEBUG_PWM_OUT
        Serial.print("Setting PWM out A to DC: ");
        Serial.println(PWM_OUT_0);
#endif
        digitalWrite(5, PWM_OUT_0);
        return;
    }

    if(duty==100)
    {
        pwmA_Dc = 1;
#ifdef DEBUG_PWM_OUT
        Serial.print("Setting PWM out A to DC: ");
        Serial.println(PWM_OUT_100);
#endif
        digitalWrite(5, PWM_OUT_100);
        return;
    }

    // normal duty cycle after DC - use setup instead
    if(pwmA_Dc)
    {
        pwmBeginA(duty);
        return;
    }

    OCR0B = PWM_OUT_VAL(duty);

#ifdef DEBUG_PWM_OUT
    Serial.print("Setting PWM out A to ");
    Serial.println(OCR0B);
#endif
}


/* -----------------------------------------------------------------------
   PWM out 2 - using timer2, controlling a fan on D3
   ----------------------------------------------------------------------- */

unsigned char pwmB_Dc = 0; /**< Is PWM B just DC mode (0 or 100%)? */


void pwmBeginB(unsigned int duty)
{
    pwmB_Dc = 0;
    TCCR2A  = 0;                              // TC2 Control Register A
    TCCR2B  = 0;                              // TC2 Control Register B
    TIMSK2  = 0;                              // TC2 Interrupt Mask Register
    TIFR2   = 0;                              // TC2 Interrupt Flag Register
    TCCR2A |= bit(COM2B1) | bit(WGM21) | bit(WGM20);  // OC2B cleared/set on match when up/down counting, fast PWM
    TCCR2B |= bit(WGM22) | bit(CS21);         // use clock/8, ie. res 0.5us
    OCR2A   = 79;                             // TOP overflow value is 80 producing PWM 80 * 0.5us = 40us = 25kHz
    pwmSetDcB(duty);                             
    pinMode(3, OUTPUT);
}


void pwmSetDcB(unsigned int duty)
{
    // Handle DC 0 and 100 as special cases
    if(duty==0)
    {
        pwmB_Dc = 1;
#ifdef DEBUG_PWM_OUT
        Serial.print("Setting PWM out B to DC: ");
        Serial.println(PWM_OUT_0);
#endif
        digitalWrite(3, PWM_OUT_0);
        return;
    }

    if(duty==100)
    {
        pwmB_Dc = 1;
#ifdef DEBUG_PWM_OUT
        Serial.print("Setting PWM out B to DC: ");
        Serial.println(PWM_OUT_100);
#endif
        digitalWrite(3, PWM_OUT_100);
        return;
    }

    // normal duty cycle after DC - use setup instead
    if(pwmB_Dc)
    {
        pwmBeginB(duty);
        return;
    }

    OCR2B = PWM_OUT_VAL(duty);

#ifdef DEBUG_PWM_OUT
    Serial.print("Setting PWM out B to ");
    Serial.println(OCR2B);
#endif
}
