#include <Arduino.h>
#include <util/delay.h>
#include "MCP9701.h"
#include "DataProcessing.h"
#include "InternalTemp.h"

float intTempExpFilterVal = TEMP_EXPFILT_INIT;

float readInternalTemp(void)
{
    unsigned int wADC;
    float temp;

    // The internal temperature has to be used with the internal reference of 1.1V.
    // Channel 8 can not be selected with the analogRead function yet.

    // Set the internal reference and mux.
    ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));

    ADCSRA |= _BV(ADEN);  // enable the ADC

    _delay_ms(20);

    ADCSRA |= _BV(ADSC);  // Start the ADC

    // Detect end-of-conversion
    while (bit_is_set(ADCSRA,ADSC))
        ;

    // Reading register "ADCW" takes care of how to read ADCL and ADCH.
    wADC = ADCW;

    // discard first reading
    ADCSRA |= _BV(ADEN);  // enable the ADC
    ADCSRA |= _BV(ADSC);  // Start the ADC
    while (bit_is_set(ADCSRA,ADSC))
        ;
    wADC = ADCW;

    // todo - temp filtering/averaging?
    temp = (wADC - AVR_INT_TEMP_offset ) / AVR_INT_TEMP_coeff;

    temp = ExpFilter(&intTempExpFilterVal, tempExpFilterWeight, temp);

    return temp;
}
