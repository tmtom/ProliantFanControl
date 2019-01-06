#include <Arduino.h>
#include <EEPROM.h>

#include "Config.h"
#include "PFCmain.h"
#include "PwmMeasure.h"
#include "MCP9701.h"

#include "EepromConfig.h"


/*
  Layout of the data stored in EEPROM:

  tempWeights:
  F1 TEMP_SENSORS * float, 1B checksum
  F2 TEMP_SENSORS * float, 1B checksum
  ...
     
  expFilterWeight
  float, float, 1B checksum
   
  mappingTable
  F1
  TempCoefs[0]  PWM_COEFFS * 1Bytye (unsigned char), 1B checksum
  TempCoefs[1]  PWM_COEFFS * 1Bytye (unsigned char), 1B checksum
  ...
  F2
  ...
  ...
     
*/

#define EE_CHECKSUM_MAGIC 0xd1


#define EE_TEMPWEIGHTS_START      0
#define EE_TEMPWEIGHTS_DATA_SIZE  (TEMP_SENSORS * sizeof(float))
#define EE_TEMPWEIGHTS_ROW_SIZE   (EE_TEMPWEIGHTS_DATA_SIZE + 1)
#define EE_TEMPWEIGHTS_END        (EE_TEMPWEIGHTS_START + (FANS * EE_TEMPWEIGHTS_ROW_SIZE))
// 28

#define eepTempWeightRowAddr(f)   (EE_TEMPWEIGHTS_START + (f)*EE_TEMPWEIGHTS_ROW_SIZE )
#define eepTempWeightCsumAddr(f)  (eepTempWeightRowAddr((f)) + EE_TEMPWEIGHTS_DATA_SIZE)


#define EE_EXPFILTER_START      (EE_TEMPWEIGHTS_END)
#define EE_EXPFILTER_PWM        (EE_EXPFILTER_START)
#define EE_EXPFILTER_PWM_SIZE   (sizeof(float))
#define EE_EXPFILTER_TEMP       (EE_EXPFILTER_PWM + EE_EXPFILTER_PWM_SIZE)
#define EE_EXPFILTER_TEMP_SIZE  (sizeof(float))
#define EE_EXPFILTER_CSUM       (EE_EXPFILTER_TEMP + EE_EXPFILTER_TEMP_SIZE)
#define EE_EXPFILTER_DATA_SIZE  (EE_EXPFILTER_PWM_SIZE + EE_EXPFILTER_TEMP_SIZE)
#define EE_EXPFILTER_SIZE       (EE_EXPFILTER_DATA_SIZE + 1)
#define EE_EXPFILTER_END        (EE_EXPFILTER_START + EE_EXPFILTER_SIZE)


#define EE_MAPPINGTABLE_START     (EE_EXPFILTER_END)
#define EE_MAPPINGTABLE_DATA_SIZE (PWM_COEFFS)
#define EE_MAPPINGTABLE_ROW_SIZE  (PWM_COEFFS + 1)
#define EE_MAPPINGTABLE_FAN_SIZE  (EE_MAPPINGTABLE_ROW_SIZE * TEMP_COEFFS)
#define EE_MAPPINGTABLE_END       (EE_MAPPINGTABLE_START + FANS * TEMP_COEFFS * EE_MAPPINGTABLE_ROW)


#define eepMapTableRowAddr(f,t)  ( EE_MAPPINGTABLE_START + (f)*EE_MAPPINGTABLE_FAN_SIZE + (t)*EE_MAPPINGTABLE_ROW_SIZE )
#define eepMapTableCsumAddr(f,t) ( eepMapTableRowAddr(f,t) + EE_MAPPINGTABLE_DATA_SIZE )


//#if EE_mappingTable_END >= 1024
//#error EEProm size overrun
//#endif

// TODO - add checksum constant to deteect empty eeprom!!!

// ------------------------- Common / aux functions --------------- 

/** 
 * Reads block of data from EEPROM and checks the checksum
 * 
 * @param addr start EEPROM address
 * @param buff buffer for the data where the last byte is the checksum
 * @param dataLen data length in bytes
 * 
 * @return zero if successful
 */
int LoadAndCheck(int addr, unsigned char *buff, int dataLen)
{
    eeprom_read_block(buff,
                      (void*)(addr),
                      dataLen);

    //check the checksum
    unsigned char sum = EE_CHECKSUM_MAGIC;
    int a;
    for(a=0; a<dataLen-1; ++a)
        sum += *(buff++);

    if(sum != *buff)
        return -1;   // checksum mismatch

    return 0;
}


// --------------------------- Temp weights -----------------------

int LoadTempWeights(int fan)
{
    unsigned char tempRow[EE_TEMPWEIGHTS_ROW_SIZE];

    if(fan<0 || fan>=FANS)
        return -1;  // fan index out of range

    if(LoadAndCheck(eepTempWeightRowAddr(fan), tempRow, EE_TEMPWEIGHTS_ROW_SIZE))
    {
#ifdef DEBUG_EEPROM_CONFIG
        Serial.println("Failed checksum in LoadTempWeights");
#endif
        return -1;   // checksum mismatch
    }
    memcpy(tempWeights[fan], tempRow, EE_TEMPWEIGHTS_DATA_SIZE);

    return 0;
}


int SaveTempWeights(int fan)
{
    unsigned char *data = NULL;
    unsigned char sum = EE_CHECKSUM_MAGIC;

    if(fan<0 || fan>=FANS)
        return -1;  // fan index out of range

    data = (unsigned char*)(&(tempWeights[fan][0]));

    for(int a=0; a<EE_TEMPWEIGHTS_DATA_SIZE; ++a)
        sum += *(data++);

    eeprom_update_block((const void*)(tempWeights[fan]),     // data
                        (void*)eepTempWeightRowAddr(fan),    // addr
                        EE_TEMPWEIGHTS_DATA_SIZE);           // size

    eeprom_update_byte((void*)eepTempWeightCsumAddr(fan),    // addr
                       sum);                                 // data
    return 0;
}


// --------------------------- PWM exp filter weight -----------------------

int LoadPwmExpFilter(void)
{
    unsigned char filtData[EE_EXPFILTER_SIZE];


    if(LoadAndCheck(EE_EXPFILTER_START, filtData, EE_EXPFILTER_SIZE))
    {
#ifdef DEBUG_EEPROM_CONFIG
        Serial.println("Failed checksum in LoadPwmExpFilter");
#endif
        return -1;
    }

    memcpy((void*)(&pwmExpFilterWeight),  filtData,                       EE_EXPFILTER_PWM_SIZE);
    memcpy((void*)(&tempExpFilterWeight), filtData+EE_EXPFILTER_PWM_SIZE, EE_EXPFILTER_TEMP_SIZE);

    return 0;
}


int SavePwmExpFilter(void)
{
    unsigned char sum = EE_CHECKSUM_MAGIC;
    int a = 0;

    unsigned char *data = (unsigned char*)(&pwmExpFilterWeight);

    for(a=0; a<EE_EXPFILTER_PWM_SIZE; ++a)
        sum += *(data++);

    data = (unsigned char*)(&tempExpFilterWeight);
    for(a=0; a<EE_EXPFILTER_TEMP_SIZE; ++a)
        sum += *(data++);


    eeprom_update_block((const void*)(&pwmExpFilterWeight), // data
                        (void*)(EE_EXPFILTER_PWM),          // addr
                        EE_EXPFILTER_PWM_SIZE);             // size

    eeprom_update_block((const void*)(&tempExpFilterWeight), // data
                        (void*)(EE_EXPFILTER_TEMP),          // addr
                        EE_EXPFILTER_TEMP_SIZE);             // size

    eeprom_update_byte((void*)(EE_EXPFILTER_CSUM),        // addr
                       sum);                              // data

    return 0;
}


// --------------------------- Mapping table -----------------------

int LoadMappingTable(int fan, int tempIdx)
{
    unsigned char mapRow[EE_MAPPINGTABLE_ROW_SIZE];

    if(fan<0 || fan>=FANS)
        return -1; // fan index out of range

    if(tempIdx<0 || tempIdx>=TEMP_COEFFS)
        return -1; // temp index out of range

    if(LoadAndCheck(eepMapTableRowAddr(fan, tempIdx), mapRow, EE_MAPPINGTABLE_ROW_SIZE))
    {
#ifdef DEBUG_EEPROM_CONFIG
        Serial.println("Failed checksum in LoadMappingTable");
#endif
        return -1;
    }

    memcpy((void*)(mappingTable[fan][tempIdx]), mapRow, EE_MAPPINGTABLE_DATA_SIZE);

    return 0;
}


int SaveMappingTable(int fan, int tempIdx)
{
    unsigned char *data = NULL;
    unsigned char sum = EE_CHECKSUM_MAGIC;

    if(fan<0 || fan>=FANS)
        return -1;

    if(tempIdx<0 || tempIdx>=TEMP_COEFFS)
        return -1;

    data = (unsigned char*)(mappingTable[fan][tempIdx]);

    for(int a=0; a<EE_MAPPINGTABLE_DATA_SIZE; ++a)
        sum += *(data++);

    eeprom_update_block((const void*)(mappingTable[fan][tempIdx]), // data
                        (void*)eepMapTableRowAddr(fan, tempIdx),   // addr
                        EE_MAPPINGTABLE_DATA_SIZE);                // size

    eeprom_update_byte((void*)eepMapTableCsumAddr(fan, tempIdx),   // addr
                       sum);                                       // data
    return 0;
}


