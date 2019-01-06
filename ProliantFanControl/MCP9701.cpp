#include <Arduino.h>
#include "DataProcessing.h"
#include "MCP9701.h"

float tempExpFilterVal[TEMP_EXT_SENSORS];
float tempExpFilterWeight  = TEMP_EXPFILT_WEIGHT;

/*
  MCP9701 conversion parameters and function
  V0 = 400mV
  tc = 19.5mV/C
  Vref = 5V
  
  Vout = analogRead / 1024 * Vref
  Vout = tc * temp + V0
  
  temp  = ((analogRead / 1024 * Vref) - V0) / tc
  temp  = analogRead / 1024 * Vref / tc - V0 / tc
  
  Or if we prepare the fixed part:
  temp = A * analogRead - B
*/

#define MCP9701_V0          0.400
#define MCP9701_tc          0.0195

// when powered by USB there is a schotky diode which results in about 4.75V
//#define MCP9701_Vref        5.0
//#define MCP9701_Vref        4.746
#define MCP9701_Vref        4.97
#define MCP9701_AD_steps 1024.0

// 0.25040064102564102564102564102564
#define MCP9701_A ((MCP9701_Vref / MCP9701_tc) / MCP9701_AD_steps)

// 20.512820512820512820512820512821
#define MCP9701_B (MCP9701_V0 / MCP9701_tc)



float readTemp(unsigned char pin)
{
    // throw away the first read
    float temp = (float)analogRead(A0 + pin);
    
    temp = ((float)analogRead(pin)) * MCP9701_A - MCP9701_B;

    temp = ExpFilter(&(tempExpFilterVal[pin]), tempExpFilterWeight, temp);

    return temp;
}
