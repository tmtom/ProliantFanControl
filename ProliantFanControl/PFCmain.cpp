/*
 * Arduino Nano
 *  Using:
 *  - timer0 - for PWM out for the first fan. NOTE that we can NOT use delay, millis, etc.
 *  - timer1 - measure duty cycle of incoming PWM. When not used (planned for) manual mode watchdog.
 *  - timer2 - for PWM out for the second fan.
 *
 * 
 * Serial for communication
 * 
 * EEPROM for mapping tables
 * 
 use internal temp??
 *
 * Keeping it in one file for now (needs refactoring).

 * PWM input od D8 !!

 temp in (MCP9701)
 a0 .. a7
 using a0, a1
 * 

 PWM out 1 - using timer 0 (cannot use delay), controlling a fan on D5
 PWM out 2 - using timer2, controlling a fan on D3



 TODO:
 - testing!!!!
 - switch from manual to auto/failsafe after "timeout" (use N reporting periods)
 - check for valid data... (EEPROM, whne switching from F to A, etc...)
 - internal temp filter, callibration commands

*/

#include <Arduino.h>
#include <EEPROM.h>

#include "PFCmain.h"
#include "Config.h"
#include "InternalTemp.h"
#include "MCP9701.h"
#include "PwmMeasure.h"
#include "PwmOut.h"
#include "EepromConfig.h"
#include "DataProcessing.h"


/*******************************************************************************
 *
 *  Main global data
 *
 ******************************************************************************/


float          tempWeights[FANS][TEMP_SENSORS];            /**< Temperature weights for each fan */
unsigned char mappingTable[FANS][TEMP_COEFFS][PWM_COEFFS]; /**< Fan PWM and temperature mapping tables */
int                  temps[TEMP_SENSORS];                  /**< Measured temperatures*/

char opMode = "A";                                         /**< Mode: A - auto, M - manual, F - failsafe */



/*******************************************************************************
 *
 *  Communication / protocol - TBD
 *
 ******************************************************************************/
#define CMD_ERR_NODATA              -1
#define CMD_ERR_SYNTAX              -2
#define CMD_ERR_SYNTAX_FAN          -3
#define CMD_ERR_SYNTAX_TEMP         -4
#define CMD_ERR_SYNTAX_TEMP_WEIGHT  -5
#define CMD_ERR_SYNTAX_FAN_PWM      -6
#define CMD_ERR_SYNTAX_PWM_VALUE    -7
#define CMD_ERR_SYNTAX_PWM_FILT     -8
#define CMD_ERR_SYNTAX_PWM_TABLE    -9
#define CMD_ERR_SYNTAX_EXTRA_DATA  -10
#define CMD_ERR_FAN_NUMBER         -11
#define CMD_ERR_SAVE_PWM_FILT      -12
#define CMD_ERR_SAVE_PWM_MAP       -13
#define CMD_ERR_SAVE_TEMP_WEIGHTS  -14
#define CMD_ERR_NOT_IMPLEMENTED   -100



#define CMD_BUFF_SIZE  160
char  serCmd[CMD_BUFF_SIZE];
int   serCmdCnt = 0;
int   checksum  = 0;


/** 
 * Parse manual mode 'F<fan_num>:<PWM_val>' (e.g. "F1:21")
 * 
 * @param s  string to parse
 * @param fan parsed fan zero based index
 * @param pwm parsed PWM value
 * 
 * @return zero when successful
 */
int parseFanPwm(char *s, unsigned char *fan, unsigned char *pwm)
{
    if(s == NULL)
        return CMD_ERR_SYNTAX;

    if(*s != 'F')
        return CMD_ERR_SYNTAX_FAN;

    ++s;
    if(*s<'1' || *s>('0'+FANS))
        return CMD_ERR_FAN_NUMBER;

    *fan = (unsigned char) (*s - '0' - 1);

    ++s;
    if(*s != ':')
        return CMD_ERR_SYNTAX_FAN_PWM;

    ++s;
    int v = atoi(s);
    if(v > 100)
	return CMD_ERR_SYNTAX_PWM_VALUE;

    *pwm = (unsigned char) v;

    return 0;
}


/** 
 * Parse 'F<n> and returns <n> (e.g. "F1" -> 1)
 * 
 * @param s string to parse
 * 
 * @return fan number from the string (not zero based index) if successfull, <0 for error
 */
int parseFan(char * s)
{
    if(s == NULL)
        return CMD_ERR_SYNTAX;

    if(*s != 'F')
        return CMD_ERR_SYNTAX_FAN;

    ++s;
    if(*s<'1' || *s>('0'+FANS))
        return CMD_ERR_FAN_NUMBER;

    return (*s - '0');
}

// T:20
int parseTemp(char * s)
{
    if(s == NULL)
        return CMD_ERR_SYNTAX_TEMP;

    if(*s != 'T')
        return CMD_ERR_SYNTAX_TEMP;

    ++s;
    if(*s != ':')
        return CMD_ERR_SYNTAX_TEMP;

    ++s;
    if(*s<'0' || *s>'9')
        return CMD_ERR_SYNTAX_TEMP;

    int t=atoi(s);
    // add debug !!!!
    return t;
}

// 0.23
float parseFloat(char * s)
{
    // TODO: detect parsing errors
    return atof(s);
}

int normalizeTemp(int t)
{
    if(t < TEMP_MIN)
        return TEMP_MIN;

    if(t > TEMP_MAX)
	return TEMP_MAX;

    return t;
}


/** 
 * Convert temperature value to mapping table index
 * 
 * @param t input temperature
 * 
 * @return table index
 */
unsigned int tempIndex(int t)
{
    if(t <= TEMP_MIN)
        return 0;
    if(t >= TEMP_MAX)
        return TEMP_COEFFS-1;

    // if(((t-TEMP_MIN) % TEMP_STEP) != 0)
    //     some warning...

    return (t-TEMP_MIN) / TEMP_STEP;
}

int tempFromIndex(int t)
{
    return TEMP_MIN + (t*TEMP_STEP);
}


// --------------------------- Generic -----------------------
int cmdVer(void)
{
    Serial.println("ProliantFanControl " VERSION);
    return 0;
}


int cmdGetCfg(void)
{
    Serial.print("Fans:");
    Serial.print(FANS);

    Serial.print(" Temps:");
    Serial.print(TEMP_SENSORS);

    Serial.print(" PWM_step:");
    Serial.print(PWM_STEP);

    Serial.print(" PWM_coeffs:");
    Serial.print(PWM_COEFFS);

    Serial.print(" Temp_min:");
    Serial.print(TEMP_MIN);

    Serial.print(" Temp_step:");
    Serial.print(TEMP_STEP);

    Serial.print(" Temp_max:");
    Serial.print(TEMP_MAX);

    Serial.print(" Temp_coeffs:");
    Serial.println(TEMP_COEFFS);
    return 0;
}


// --------------------------- Temp weights -----------------------

// GetTempWeights F1
int cmdGetTempWeights(void)
{
    char *p = strtok(NULL, " ");

    int f = parseFan(p);
    if(f < 0)
        return f;

    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    Serial.print("F");
    Serial.print(f);
    Serial.print(" ");

    --f;
    for(int i=0; i<TEMP_SENSORS; ++i)
    {
        Serial.print(tempWeights[f][i], 4); // using 4 decimal places
        if(i<(TEMP_SENSORS-1))
            Serial.print(" ");
    }
    Serial.println();
    return 0;
}

// TODO normalization (sum must be 1)
// SetTempWeights F1 0.2 0.3 0.5
int cmdSetTempWeights(void)
{
    char *p = strtok(NULL, " ");

    int f = parseFan(p);
    if(f < 0)
        return f;

    --f;

    float coeffs[TEMP_SENSORS];
    for(int a=0; a<TEMP_SENSORS; ++a)
    {
        p = strtok(NULL, " ");
        if(p == NULL)
            return CMD_ERR_SYNTAX_TEMP_WEIGHT;

        coeffs[a] = parseFloat(p);
        if(coeffs[a] < 0.0)
            return (int)coeffs[a];
    }

    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    for(int a=0; a<TEMP_SENSORS; ++a)
        tempWeights[f][a] = coeffs[a];

    Serial.println("OK");
    return 0;
}

// SaveTempWeights F1
int cmdSaveTempWeights(void)
{
    char *p = strtok(NULL, " ");

    int f = parseFan(p);
    if(f < 0)
        return f;

    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    --f;


    if(SaveTempWeights(f))
	return CMD_ERR_SAVE_TEMP_WEIGHTS;

    Serial.println("OK");
    return 0;
}


// --------------------------- PWM filt -----------------------

// GetPwmFilt
int cmdGetPwmFilt(void)
{
    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    Serial.print(pwmExpFilterWeight, 4);
    Serial.print(" ");
    Serial.println(tempExpFilterWeight, 4);
    return 0;
}

// SetPwmFilt 30 0.1
int cmdSetPwmFilt(void)
{
    char *p = strtok(NULL, " ");
    if(p == NULL)
        return CMD_ERR_SYNTAX_PWM_FILT;

    float pwmW = parseFloat(p);

    if(pwmW<0.0 || pwmW>1.0)
        return CMD_ERR_SYNTAX_PWM_FILT;


    p = strtok(NULL, " ");

    if(p == NULL)
        return CMD_ERR_SYNTAX_PWM_FILT;

    float tempW = parseFloat(p);

    if(tempW<0.0 || tempW>1.0)
        return CMD_ERR_SYNTAX_PWM_FILT;

    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    pwmExpFilterWeight  = pwmW;
    tempExpFilterWeight = tempW;

#ifdef DEBUG_CMD_PROC
    Serial.print(pwmW , 4);
    Serial.print(" ");
    Serial.println(tempW, 4);
#endif

    Serial.println("OK");
    return 0;
}

// SavePwmFilt
int cmdSavePwmFilt(void)
{
    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    if(SavePwmExpFilter())
      return CMD_ERR_SAVE_PWM_FILT;

    Serial.println("OK");
    return 0;
}

// --------------------------- Mapping table -----------------------

// GetPwmMap F1 T:20
int cmdGetPwmMap(void)
{
    char *p = strtok(NULL, " ");

    int fan = parseFan(p);
    if(fan < 0)
        return fan;

    p = strtok(NULL, " ");
    if(p == NULL) // early end..
        return CMD_ERR_SYNTAX_TEMP;

    int t = parseTemp(p);

    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    int tIdx = tempIndex(t);

    Serial.print("F");
    Serial.print(fan);
    Serial.print(" T:");
    Serial.print(tempFromIndex(tIdx)); // map back to show what temp we actually use (in case the request was not in the grid/step)
    Serial.print(" ");

    --fan;

    for(int pc=0; pc<PWM_COEFFS; ++pc)
    {
        Serial.print(mappingTable[fan][tIdx][pc]);
        if(pc<(PWM_COEFFS-1))
            Serial.print(" ");
    }
    Serial.println();

    return 0;
}

// SetPwmMap F1 T:20 0 10 15 20 25 30 35 40 45 50 55 60 65 70 75 80 85 90 95 100
int cmdSetPwmMap(void)
{
    char *p = strtok(NULL, " ");

    int fan = parseFan(p);
    if(fan < 0)
        return fan;
    --fan;

    p = strtok(NULL, " ");
    if(p == NULL) // expected temp. token...
        return CMD_ERR_SYNTAX_TEMP;

    int t = parseTemp(p);

    t = tempIndex(t);

    unsigned char pMap[PWM_COEFFS];
    int tmp=0;
    for(int a=0; a<PWM_COEFFS; ++a)
    {
        p = strtok(NULL, " ");
        if(p == NULL) // expected pwm token...
            return CMD_ERR_SYNTAX_PWM_TABLE;
        
        tmp = atoi(p);

#ifdef DEBUG_CMD_PROC
	Serial.print(a);
	Serial.print(": ");
	Serial.println(tmp);
	Serial.println(p);
#endif

        if(tmp<0 || tmp>100)
            return CMD_ERR_SYNTAX_PWM_TABLE;
        
        pMap[a] = (unsigned char) tmp;
    }

    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    for(int a=0; a<PWM_COEFFS; ++a)
        mappingTable[fan][t][a] = pMap[a];

    Serial.println("OK");
    return 0;
}

// SavePwmMap F1 T:20 
int cmdSavePwmMap(void)
{
    char *p = strtok(NULL, " ");

    int f = parseFan(p);
    if(f < 0)
        return f;
    --f;

    p = strtok(NULL, " ");
    if(p == NULL) // early end..
        return CMD_ERR_SYNTAX_TEMP;

    int t = parseTemp(p);

    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    t = tempIndex(t);

    if(SaveMappingTable(f, t))
      return CMD_ERR_SAVE_PWM_MAP;

    Serial.println("OK");
    return 0;
}


// --------------------------- Op modes -----------------------

unsigned char manualPwm[FANS];

// ModeManual F1:20 F2:30
int cmdModeManual(void)
{
    char *p = NULL;
    unsigned char pwm[FANS];

    unsigned char fan;
    int rc = -1;

    for(int a=0; a<FANS; ++a)
    {
	p = strtok(NULL, " ");
	if(p==NULL)
	    return CMD_ERR_SYNTAX_FAN_PWM; // out of order...

	rc = parseFanPwm(p, &fan, &(pwm[a]));
	if(rc != 0)
	    return rc;

	if(fan != a)
	    return CMD_ERR_SYNTAX_FAN_PWM; // out of order...

#ifdef DEBUG_CMD_PROC
	Serial.print(a);
	Serial.print(": ");
	Serial.println(fan);
	Serial.print(" ");
	Serial.println(pwm[a]);
#endif
    }

    for(int a=0; a<FANS; ++a)
	manualPwm[a] = pwm[a];

    opMode = 'M';

    Serial.println("OK");
    return 0;
}


int cmdModeAuto(void)
{
    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    // TODO: check if we have all the data (eeprom checksum) and then confirm??
    //....
    opMode = 'A';
    Serial.println("OK");
    return 0;
}


int cmdModeFailsafe(void)
{
    if(strtok(NULL, " ") != NULL) // another token?
        return CMD_ERR_SYNTAX_EXTRA_DATA;

    opMode = 'F';
    Serial.println("OK");
    return 0;
}


// ----------------------------------------------------------------------------------------------------------------------------

/** 
 * Reads any data up to the new line from the serial
 * 
 * 
 * @return 1 of we have a complete line (in serCmd)
 */
int ReadSerialLine(void)
{
    // default Arduino serial buffer is 64B Tx and 64B Rx !!!
    // it may still overflow!!!!
    int c;
    while((c=Serial.read()) >= 0)
    {
        if(serCmdCnt < 0) // error mode / overflow, just skip till the end of line
        {
            if(c == '\n')
            {
                serCmdCnt = 0; // back to normal...
                break;
            }
        }
        else
        {
            if(c == '\n')
            {
                serCmd[serCmdCnt] = '\0';
                return 1;
            }
            else
            {
                serCmd[serCmdCnt++] = c;

                // buffer overflow - would not fit even the terminating '\0', indicate error mode
                if(serCmdCnt >= CMD_BUFF_SIZE)
		{
		    Serial.println("E Buffer overflow");
                    serCmdCnt = -1;
		}
            }
        }
        
    }
    return 0;
}



void errorResponse(int e)
{
    switch(e)
    {
    case CMD_ERR_NODATA:
        Serial.println("E No data");
        break;

    case CMD_ERR_SYNTAX:
        Serial.println("E Syntax error");
        break;

    case CMD_ERR_SYNTAX_FAN:
        Serial.println("E Syntax error (fan)");
        break;

    case CMD_ERR_SYNTAX_TEMP:
        Serial.println("E Syntax error (temperature)");
        break;

    case CMD_ERR_SYNTAX_TEMP_WEIGHT:
        Serial.println("E Syntax error (temperature weight)");
        break;

    case CMD_ERR_SYNTAX_FAN_PWM:
        Serial.println("E Syntax error (fan/pwm)");
        break;

    case CMD_ERR_SYNTAX_PWM_VALUE:
        Serial.println("E Syntax error (PWM value)");
        break;

    case CMD_ERR_SYNTAX_PWM_FILT:
        Serial.println("E Syntax error (PWM filter)");
        break;

    case CMD_ERR_SYNTAX_PWM_TABLE:
        Serial.println("E Syntax error (PWM table)");
        break;

    case CMD_ERR_SYNTAX_EXTRA_DATA:
        Serial.println("E Syntax error (extra data/token)");
        break;

    case CMD_ERR_FAN_NUMBER:
        Serial.println("E Wrong fan number");
        break;

    case CMD_ERR_SAVE_PWM_FILT:
        Serial.println("E Saving PWM filter params");
        break;

    case CMD_ERR_SAVE_PWM_MAP:
        Serial.println("E Saving PWM map");
        break;

    case CMD_ERR_SAVE_TEMP_WEIGHTS:
        Serial.println("E Saving temp. weights");
        break;

    case CMD_ERR_NOT_IMPLEMENTED:
        Serial.println("E Not implemented yet");
        break;
        
    default:
        break;
    }
}


int ParseAndExecute(void)
{
    // TODO: check and remove checksum

    char *cmd = strtok(serCmd, " ");


    if(cmd == NULL) // empty line? ignore
        return CMD_ERR_NODATA;

#ifdef DEBUG_CMD_PROC
    Serial.println(cmd);
#endif

// generic
    if(!strcmp(cmd, "Ver"))
        return cmdVer();

    if(!strcmp(cmd, "GetCfg"))
        return cmdGetCfg();

// temp weights
    if(!strcmp(cmd, "GetTempWeights"))
        return cmdGetTempWeights();

    if(!strcmp(cmd, "SetTempWeights"))
        return cmdSetTempWeights();

    if(!strcmp(cmd, "SaveTempWeights"))
        return cmdSaveTempWeights();

// pwm measure
    if(!strcmp(cmd, "GetPwmFilt"))
        return cmdGetPwmFilt();

    if(!strcmp(cmd, "SetPwmFilt"))
        return cmdSetPwmFilt();

    if(!strcmp(cmd, "SavePwmFilt"))
        return cmdSavePwmFilt();

// mapping table
    if(!strcmp(cmd, "GetPwmMap"))
        return cmdGetPwmMap();
    
    if(!strcmp(cmd, "SetPwmMap"))
        return cmdSetPwmMap();

    if(!strcmp(cmd, "SavePwmMap"))
        return cmdSavePwmMap();

// operational mode
    if(!strcmp(cmd, "ModeManual"))
        return cmdModeManual();

    if(!strcmp(cmd, "ModeAuto"))
        return cmdModeAuto();

    if(!strcmp(cmd, "ModeFailsafe"))
        return cmdModeFailsafe();

// TBD temp callibration....

// anything else
    return CMD_ERR_SYNTAX;
}





/*******************************************************************************
 *
 *  MAIN
 *
 ******************************************************************************/

// Vars for loop()
int duty      = 0;
int loopCount = 0;

int newTemp   = -1;
int newPwmA   = -1;
int newPwmB   = -1;

unsigned char ledBlink = 0;



/* -----------------------------------------------------------------------
   Setup
   ----------------------------------------------------------------------- */
void pfcSetup()
{
    TIMSK0 = 0; // we do not use interrupts on timer0, using it just as a PWM out
    TIMSK2 = 0; // we do not use interrupts on timer2, using it just as a PWM out

    TIMSK1 = 0; // PWM measurement function will set it itself as needed

    // At higher speed we sometimes miss a character (disabled interrupts during PWM measurement).
    // Lower speed seems ot be OK...

    Serial.begin(19200);

    //Serial.begin(57600);
    //Serial.begin(115200);

    // set PWM input as input, internal 20k pullup
    pinMode(8, INPUT_PULLUP);

    pinMode(LED_PIN, OUTPUT);

    // no need to set temp measurment pins as inputs here

    // Immediately start all fans ... at PWM_MAPPING_TABLE_DEFAULT
    pwmBeginA(PWM_MAPPING_TABLE_DEFAULT);
#if FANS > 1
    pwmBeginB(PWM_MAPPING_TABLE_DEFAULT);
#endif

    // initialize everything before trying to load from EEPROM to have some baseline
    int a,b,c;
    for(a=0; a<FANS; ++a)
    {
        for(b=0; b<TEMP_SENSORS; ++b)
            tempWeights[a][b] = 0.0;

        for(b=0; b<TEMP_COEFFS; ++b)
            for(c=0; c<PWM_COEFFS; ++c)
                mappingTable[a][b][c] = PWM_MAPPING_TABLE_DEFAULT;
    }


    cmdVer();
    cmdGetCfg();
    //cmdBuffEnd = &(serCmd[0]);

    opMode='A';

    if(LoadPwmExpFilter())
    {
        Serial.println("*E EEPROM checksum mismatch (PWM exp. filter). Using failsafe mode.");
        opMode='F';
        return;
    }

    // fixed startup value is enough
    pwmExpFilterVal = PWM_EXPFILT_INIT;

    for(int a=0; a<TEMP_EXT_SENSORS; ++a)
	tempExpFilterVal[a] = TEMP_EXPFILT_INIT;

    intTempExpFilterVal = TEMP_EXPFILT_INIT;

    for(int fan=0; fan<FANS; ++fan)
        if(LoadTempWeights(fan))
        {
            Serial.print("*E EEPROM checksum mismatch (temp. weights F:");
            Serial.print(fan+1);
            Serial.println("). Using failsafe mode.");
            opMode='F';
            return;
        }

    for(int fan=0; fan<FANS; ++fan)
        for(int temp=0; temp<TEMP_COEFFS; ++temp)
            if(LoadMappingTable(fan, temp))
            {
                Serial.print("*E EEPROM checksum mismatch (PWM mapping table F:");
                Serial.print(fan+1);
                Serial.print(" T:");
                Serial.print(tempFromIndex(temp));
                Serial.println("). Using failsafe mode.");
                opMode='F';
                return;
            }
}



/* -----------------------------------------------------------------------
   Loop
   ----------------------------------------------------------------------- */
void pfcLoop()
{
    pwmMeasureBegin();
    while(pwmMeasureComplete())
        ;

    duty = int(pwmDuty + 0.5);

#ifdef DEBUG_LOOP
    Serial.print("Filt ");
    Serial.print(pwmDuty);
    Serial.print(", duty ");
    Serial.println(duty);
#endif

    // read all the temperature(s)
    temps[0] = readInternalTemp();

    for(unsigned char a=1; a<TEMP_SENSORS; ++a)
    {
        // we are using A0, A1
        temps[a] = readTemp(a - 1);
    }


    // depedning on the opMode set the output PWM

    // Failsafe mode - just copy input PWM to the outputs
    if(opMode == 'F')
    {
        newPwmA = duty;
        pwmSetDcA(newPwmA);
#if FANS > 1
        newPwmB = duty;
        pwmSetDcB(newPwmB);
#endif
    }
    else
    {
        // Auto mode - do the mapping magic from inputs PWM and temperatures to the PWM outputs
        if(opMode == 'A')
        {
            // fan A aka fan=0
            newTemp = normalizeTemp(averageTemp(0));
#ifdef DEBUG_DATA_PROCESSING
	    Serial.println(newTemp);
#endif
            newPwmA = interpolatePwm(0, duty, newTemp);
            pwmSetDcA(newPwmA);

#if FANS > 1
            // fan B aka fan=0
            newTemp = normalizeTemp(averageTemp(1));
#ifdef DEBUG_DATA_PROCESSING
	    Serial.println(newTemp);
#endif
            newPwmB = interpolatePwm(1, duty, newTemp);
            pwmSetDcB(newPwmB);
#endif
        }
        else
        {
            if(opMode == 'M')
            {
		newPwmA = manualPwm[0];
		pwmSetDcA(newPwmA);
#if FANS > 1
		newPwmB = manualPwm[1];
		pwmSetDcB(newPwmB);
#endif
            }
            else
            {
                // fatal error
                Serial.println("*E Invalid opmode, setting failsafe");
		opMode = 'F';
            }
        }
    }


    // In every measure/adjust cycle switch the green LED. If everything is OK it should be blinking fairly quickly
    if(ledBlink)
    {
        digitalWrite(LED_PIN, HIGH);
        ledBlink = 0;
    }
    else
    {
        digitalWrite(LED_PIN, LOW);
        ledBlink = 1;
    }

    // periodic reports
    if(loopCount >= REPORT_FREQ)
    {

#ifndef NO_REPORTS
        // Autonomous mode:
        // *A PWM1_in:20 T1_in:23 T2_in:30 F1_out:20 F2_out:30
        Serial.print("*");
        Serial.print(opMode);
        Serial.print(" PWM1_in:");
        Serial.print(duty);

        for(unsigned char a=0; a<TEMP_SENSORS; ++a)
        {
            Serial.print(" T");
            Serial.print(a);
            Serial.print("_in:");
            Serial.print(temps[a]);
        }

        Serial.print(" F1_out:");
        Serial.print(newPwmA);

#if FANS > 1
        Serial.print(" F2_out:");
        Serial.println(newPwmB);
#else
        Serial.println();
#endif        
#endif        
        loopCount = 0;
    }
    else
        ++loopCount;
        
    // handle serial comms...
    if(ReadSerialLine())
    { // we have a line
        int res = ParseAndExecute();
        if(res != 0)
            errorResponse(res);
        serCmdCnt = 0;
    }
}

/* -----------------------------------------------------------------------
   Main
   ----------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    init(); // Arduino standard init
    pfcSetup();

    while(1)
        pfcLoop();

    return 0;
}
