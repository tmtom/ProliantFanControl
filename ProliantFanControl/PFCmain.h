#ifndef __PFCMAIN_H__
#define __PFCMAIN_H__

#include "Config.h"

/*******************************************************************************
 *
 *  Main global data
 *
 ******************************************************************************/

// Internal sensor has index 0, so TEMP_SENSORS+1
#define TEMP_SENSORS (TEMP_EXT_SENSORS+1)

// for example temp range 20-70, step 5 -> 11 coeffs
#define TEMP_COEFFS  (((TEMP_MAX - TEMP_MIN) / TEMP_STEP) + 1)

// results in 21 coeffs
#define PWM_COEFFS   ((100 / PWM_STEP) + 1)

/* Fans are zero based
   Temp sensors are zero based, 0 is internal temp, first external temp is 1
*/

extern float          tempWeights[FANS][TEMP_SENSORS];            /**< Temperature weights for each fan */
extern unsigned char mappingTable[FANS][TEMP_COEFFS][PWM_COEFFS]; /**< Fan PWM and temperature mapping tables */
extern int                  temps[TEMP_SENSORS];                  /**< Measured temperatures*/

#endif // __PFCMAIN_H__
