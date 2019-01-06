#ifndef __EEPROMCONFIG_H__
#define __EEPROMCONFIG_H__

// --------------------------- Temp weights -----------------------

/** 
 * Load temperature weights for the given fan
 * 
 * @param fan fan number
 * 
 * @return zero when successful
 */
int LoadTempWeights(int fan);


/** 
 * Save temp weights for fan to EEPROM
 * 
 * @param fan zero based fan index
 *
 * @return zero when successful
 */
int SaveTempWeights(int fan);


// --------------------------- PWM exp filter weight -----------------------

/** 
 * Load PWM exp filter params (starting value and wight)
 * 
 * @return zero when successful
 */
int LoadPwmExpFilter(void);


/** 
 * Save PWM exp filter params (starting value and wight)
 *
 * @return zero when successful
 */
int SavePwmExpFilter(void);


// --------------------------- Mapping table -----------------------

/** 
 * Save PWM mapping table for the given fan and temp index
 * 
 * @param fan 
 * @param temp 
 * 
 * @return zero when successful
 */
int LoadMappingTable(int fan, int tempIdx);


/** 
 * Save PWM mapping tabel for given fan and temp index
 * 
 * @param fan zero based fan index
 * @param temp zero based temperature INDEX
 *
 * @return zero when successful
 */
int SaveMappingTable(int fan, int tempIdx);


// ------------------------- TODO - temp callibration coeffs --------

#endif // __EEPROMCONFIG_H__
