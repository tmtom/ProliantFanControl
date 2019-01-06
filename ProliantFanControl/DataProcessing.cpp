#include <Arduino.h>
#include "DataProcessing.h"
#include "PFCmain.h"


float ExpFilter(float *oldVal, float weight, float newVal)
{
    *oldVal = (weight * newVal) + ((1.0-weight ) * (*oldVal));
    return *oldVal;
}


int averageTemp(unsigned char fan)
{
    float sum    = 0.0;
    float wSum   = 0.0;
    int   result = 0;

    for(int a=0; a<TEMP_SENSORS; ++a)
    {
        sum += tempWeights[fan][a] * temps[a];
        wSum += tempWeights[fan][a];
    }
    result = (int)(sum / wSum);

    if(result < TEMP_MIN)
        result = TEMP_MIN;
    else
    {
        if(result > TEMP_MAX)
            result = TEMP_MAX;
    }
    return result;
}


int interpolatePwm(unsigned char fan, unsigned char pwm, unsigned char tmp)
{
    tmp -= TEMP_MIN; // normalize to start from 0

    unsigned char idxTmp1 = tmp / TEMP_STEP;
    unsigned char distTmp = tmp % TEMP_STEP;
    unsigned char idxTmp2 = idxTmp1;

    if(distTmp != 0)
        ++idxTmp2;

#ifdef DEBUG_DATA_PROCESSING
    Serial.print(fan);
    Serial.print(": ");
    Serial.print(tmp);
    Serial.print(", ");
    Serial.print(idxTmp1);
    Serial.print(", ");
    Serial.print(distTmp);
    Serial.print(", ");
    Serial.print(idxTmp2);
    Serial.print(" | ");
#endif

    unsigned char idxPwm1 = pwm / PWM_STEP;
    unsigned char distPwm = pwm % PWM_STEP;
    unsigned char idxPwm2 = idxPwm1;

    if(distPwm != 0)
        ++idxPwm2;

#ifdef DEBUG_DATA_PROCESSING
    Serial.print(idxPwm1);
    Serial.print(", ");
    Serial.print(distPwm);
    Serial.print(", ");
    Serial.print(idxPwm2);
    Serial.print(" | ");
#endif

    float pwmVal1 = ( (float)(mappingTable[fan][idxTmp1][idxPwm1]) * (float)(PWM_STEP-distPwm) 
                      +
                      (float)(mappingTable[fan][idxTmp1][idxPwm2]) * (float)distPwm )
        / (float)(PWM_STEP);

    float pwmVal2 = ((float)(mappingTable[fan][idxTmp2][idxPwm1]) * (float)(PWM_STEP-distPwm)
                     +
                     (float)(mappingTable[fan][idxTmp2][idxPwm2]) * (float)distPwm )
        / (float)(PWM_STEP);

    float pwmOut = (pwmVal1 * (float)(TEMP_STEP-distTmp) + pwmVal2 * (float)distTmp) / (float)(TEMP_STEP);

#ifdef DEBUG_DATA_PROCESSING
    Serial.print(pwmVal1, 4);
    Serial.print(", ");
    Serial.print(pwmVal2, 4);
    Serial.print(", ");
    Serial.println(pwmOut, 4);
#endif

    return (int) (pwmOut+0.5);
}
