#ifndef __MCP9701_H__
#define __MCP9701_H__

#include "Config.h"

/*******************************************************************************
 *
 *  Filtering - exponential filter used for temperature
 *
 ******************************************************************************/

extern float tempExpFilterVal[TEMP_EXT_SENSORS]; /**< Current filter value */
extern float tempExpFilterWeight;                /**< Weight for the exponential filter, Let's make it "slower".  */


/** 
 * Read temperature in C from a MCP9071 sensor on the given analog pin
 * 
 * @param pin analog pin number (0..7)
 * 
 * @return temperature in C
 */
float readTemp(unsigned char pin);


#endif // __MCP9701_H__
