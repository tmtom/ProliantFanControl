#!/usr/bin/env python2

# https://pythonhosted.org/pyserial/pyserial.html

from __future__ import print_function

import serial
import io

import logging
import logging.handlers
import sys

import time
import json

import re


class FanController(object):

    def __init__(self, comPort='COM6', timeout=2, waitForReset=False):
        self.numFans       = -1
        self.numTemps      = -1

        self.tempWeights   = None
        self.pwmMap        = None

        self.pwmStepSize   = -1
        self.pwmNumCoeffs  = -1

        self.pwmWeight     = -1.0
        self.tempWeight    = -1.0

        self.tempStep      = -1
        self.tempMin       = -1
        self.tempMax       = -1
        self.tempNumCoeffs = -1

        self.comPort       = comPort
        self.timeout       = timeout
        self.waitForReset  = waitForReset
        self.serPort       = None


    def Open(self):
        try:
            self.serPort = serial.Serial(port=self.comPort, baudrate=19200, timeout=self.timeout)
        except serial.serialutil.SerialException as ex:
            logging.error('Cannot open serial port: "{}"'.format(ex))
            return False

        logging.debug('Port "{}" open'.format(self.comPort))
        if self.waitForReset:
            logging.debug('Wait for Arduino boot after reset'.format(self.comPort))
            time.sleep(3)
        else:
            # read line to be sure we get rid of any syntax error response due to some garbage sent earlier
            self.serPort.flush()
            self.serPort.write('\n')
            self.serPort.readline()

        self.serPort.reset_input_buffer()
        self.serPort.flush()
        logging.debug('We are ready')

        if not self.GetHwConfig():
            logging.error('GetHwConfig failed')
            return False

        # initialize the 2 dim array
        self.tempWeights = [[-1.0 for x in range(self.numTemps)] for y in range(self.numFans)]

        # initialize the 3 dim array
        self.pwmMap = [[[-1.0 for x in range(self.pwmNumCoeffs)] for y in range(self.tempNumCoeffs)] for z in range(self.numFans)]

        return True


    def ReadAll(self):
        if not self.GetPwmFilt():
            logging.error('GetPwmFilt failed')
            return False

        if not self.GetAllTempWeights():
            logging.error('GetAllTempWeights failed')
            return False

        if not self.GetPwmMapAll():
            logging.error('GetPwmMapAll failed')
            return False

        return True

    def Dump(self, fileName=None):
	if fileName:
	    f = open(fileName, 'w')
	    logging.info('Using "{}" for dump output'.format(fileName))
	else:
	    f = sys.stdout
	    logging.debug('Will dump to stdout')
	    
        data = {}
        print('Num_Fans:    {}'.format(self.numFans),       file=f)
        print('Num_Temps:   {}'.format(self.numTemps),      file=f)
        print('PWM_step:    {}'.format(self.pwmStepSize),   file=f)
        print('PWM_coeffs:  {}'.format(self.pwmNumCoeffs),  file=f)
        print('Filt_PWM:    {}'.format(self.pwmWeight),     file=f)
        print('Filt_temp:   {}'.format(self.tempWeight),    file=f)
        print('Temp_step:   {}'.format(self.tempStep),      file=f)
        print('Temp_min:    {}'.format(self.tempMin),       file=f)
        print('Temp_max:    {}'.format(self.tempMax),       file=f)
        print('Temp_coeffs: {}'.format(self.tempNumCoeffs), file=f)

        print('TempWeights:', file=f)
        for fan in range(self.numFans):
            print('  Fan_{}: '.format(fan+1) + ', '.join(str(x) for x in self.tempWeights[fan]), file=f)

        print('PwmMap:', file=f)
        for fan in range(self.numFans):
            print('  Fan_{}: '.format(fan+1), file=f)
            for t in range(self.tempNumCoeffs):
                print('    {}: '.format(self.Idx2Tmp(t)) + ', '.join(str(x) for x in self.pwmMap[fan][t]), file=f)

	if fileName:
	    f.close()

    def DumpJson(self, fileName=None):
        data = {}
        data['Num_Fans']    = self.numFans
        data['Num_Temps']   = self.numTemps
        data['PWM_step']    = self.pwmStepSize
        data['PWM_coeffs']  = self.pwmNumCoeffs
        data['Filt_PWM']    = self.pwmWeight
        data['Filt_temp']   = self.tempWeight
        data['Temp_step']   = self.tempStep
        data['Temp_min']    = self.tempMin
        data['Temp_max']    = self.tempMax
        data['Temp_coeffs'] = self.tempNumCoeffs

        tw = {}
        for fan in range(self.numFans):
            tw['Fan_{}'.format(fan+1)] = self.tempWeights[fan]

        data['TempWeights'] = tw

        m = {}
        for fan in range(self.numFans):
            tp = {}
            for t in range(self.tempNumCoeffs):
                tp[self.Idx2Tmp(t)] = self.pwmMap[fan][t]

            m['Fan_{}'.format(fan+1)] = tp
            
        data['PWM_map'] = m

	if fileName:
	    f = open(fileName, 'w')
	    logging.info('Using "{}" for dump JSON output'.format(fileName))
	else:
	    f = sys.stdout
	    logging.debug('Will dump JSON to stdout')

	# someheuristic to have compact inner arrays...
	jStr = json.dumps(data, sort_keys=True, indent=2)
	output2 = re.sub(r'\[\s+', '[ ', jStr)
	output3 = re.sub(r'\s+\]', ' ]', output2)
	output4 = re.sub(r',\s+(?P<num>\d\.?\d*)', ', \g<num>', output3)
	
        print(output4, file=f)

	if fileName:
	    f.close()

        return True

    def LoadJson(self, fileName, save=False):
	f = open(fileName, 'r')
	data = json.load(f)
	logging.info('Loading JSON from "{}"'.format(fileName))

	#--- Filt ---
	pwmFiltVal = None
	pwmFilt = data.get('Filt_PWM')
	if pwmFilt:
	    pwmFiltVal = float(pwmFilt)
	    if pwmFiltVal>=0.0 and pwmFiltVal<=1.0:
		logging.debug('Got Filt_PWM = {}'.format(pwmFiltVal))
		# some action
		self.pwmWeight = pwmFiltVal
		
	    else:
		logging.error('Filt_PWM out of range: {}'.format(val))
		f.close()
		return False

	tempFiltVal = None
	tempFilt = data.get('Filt_temp')
	if tempFilt:
	    tempFiltVal = float(tempFilt)
	    if tempFiltVal>=0.0 and tempFiltVal<=1.0:
		logging.debug('Got Filt_temp = {}'.format(tempFiltVal))
		# some action
		self.tempWeight = tempFiltVal
		
	    else:
		logging.error('Filt_temp out of range: {}'.format(val))
		f.close()
		return False

	if pwmFiltVal or tempFiltVal:
	    if not self.SetPwmFilt(self.pwmWeight, self.tempWeight) or not self.GetPwmFilt():
		f.close()
		return False

	    if save:
		if not self.SavePwmFilt():
		    f.close()
		    return False

	#--- TempWeights ---
	tWeights = data.get('TempWeights')
	if tWeights:
	    for fan in range(self.numFans):
		tWVals = tWeights.get('Fan_{}'.format(fan+1))
		if len(tWVals) == self.numTemps:

		    tVals = []
		    for v in tWVals:
			iVal = float(v)
			if iVal>=0.0 and iVal<=1.0:
			    tVals.append(iVal)
			else:
			    logging.error('Tempweight {} out of range'.format(v))
			    f.close()
			    return False

		    logging.debug('Got temp weights for fan {}: {}'.format(fan+1, ', '.join(str(x) for x in tVals)))
		    # some action
		    if not self.SetTempWeights(fan+1, tVals) or not self.GetTempWeights(fan+1):
			f.close()
			return False

		    if save:
			if not self.SaveTempWeights(fan+1):
			    f.close()
			    return False

		else:
		    logging.error('Wrong number of temp weights')
		    f.close()
		    return False


	#--- PWM_map ---
	pwmMap = data.get('PWM_map')
	if pwmMap:
	    for fan in range(self.numFans):
		pMapTbl = pwmMap.get('Fan_{}'.format(fan+1))
		if pMapTbl:

		    tempKeys = sorted(pMapTbl.keys())

		    for temp in tempKeys:
		    
			iTemp = int(temp)
			pwmVals = pMapTbl[temp]
			if len(pwmVals) == self.pwmNumCoeffs:

			    pVals = []
			    for v in pwmVals:
				iVal = int(v)
				if iVal>=0 and iVal<=100:
				    pVals.append(iVal)
				else:
				    logging.error('PWM value {} out of range'.format(v))
				    f.close()
				    return False

			    logging.debug('Got PWM map fan {} and temp {}: {}'.format(fan+1,
										      iTemp,
										      ', '.join(str(x) for x in pVals)))
			    # some action
			    if not self.SetPwmMap(fan+1, iTemp, pVals) or not self.GetPwmMap(fan+1, iTemp):
				f.close()
				return False

			    if save:
				if not self.SavePwmMap(fan+1, iTemp):
				    f.close()
				    return False

			else:
			    logging.error('Wrong number of PWM values')
			    f.close()
			    return False

	f.close()
	return True


    def VerifyJson(self, fileName):
	f = open(fileName, 'r')
	data = json.load(f)
	logging.info('Verifying against JSON from "{}"'.format(fileName))

	rc = True

	#--- Filt ---
	pwmFiltVal = None
	pwmFilt = data.get('Filt_PWM')
	if pwmFilt:
	    pwmFiltVal = float(pwmFilt)
	    if pwmFiltVal>=0.0 and pwmFiltVal<=1.0:
		# some action
		if self.pwmWeight != pwmFiltVal:
		    logging.error('Filt_PWM mismatch: JSON {} != ctrl {}'.format(pwmFiltVal, self.pwmWeight))
		    rc = False

		logging.debug('Verified Filt_PWM = {}'.format(pwmFiltVal))
		
	    else:
		logging.error('Filt_PWM out of range: {}'.format(val))
		f.close()
		return False

	tempFiltVal = None
	tempFilt = data.get('Filt_temp')
	if tempFilt:
	    tempFiltVal = float(tempFilt)
	    if tempFiltVal>=0.0 and tempFiltVal<=1.0:
		# some action
		if self.tempWeight != tempFiltVal:
		    logging.error('Filt_temp mismatch: JSON {} != ctrl {}'.format(tempFiltVal, self.tempWeight))
		    rc = False

		logging.debug('Verified Filt_temp = {}'.format(tempFiltVal))
		
	    else:
		logging.error('Filt_temp out of range: {}'.format(val))
		f.close()
		return False

	#--- TempWeights ---
	tWeights = data.get('TempWeights')
	if tWeights:
	    for fan in range(self.numFans):
		tWVals = tWeights.get('Fan_{}'.format(fan+1))
		if len(tWVals) == self.numTemps:

		    tVals = []
		    i = 0
		    for v in tWVals:
			iVal = float(v)
			tVals.append(iVal)
			if iVal>=0.0 and iVal<=1.0:
			    if iVal != self.tempWeights[fan][i]:
				logging.error('TempWeights mismatch Fan_{} [{}]: JSON {} != ctrl {}'.format(fan+1, i, iVal, self.tempWeights[fan][i]))
				rc = False

			else:
			    logging.error('Tempweight {} out of range'.format(v))
			    f.close()
			    return False

			i = i+1

		    logging.debug('Verified temp weights for Fan_{}: {}'.format(fan+1, ', '.join(str(x) for x in tVals)))

		else:
		    logging.error('Wrong number of temp weights')
		    f.close()
		    return False


	#--- PWM_map ---
	pwmMap = data.get('PWM_map')
	if pwmMap:
	    for fan in range(self.numFans):
		pMapTbl = pwmMap.get('Fan_{}'.format(fan+1))
		if pMapTbl:

		    tempKeys = sorted(pMapTbl.keys())

		    for temp in tempKeys:
		    
			iTemp = int(temp)
			pwmVals = pMapTbl[temp]
			if len(pwmVals) == self.pwmNumCoeffs:

			    pVals = []
			    i = 0
			    for v in pwmVals:
				iVal = int(v)
				if iVal>=0 and iVal<=100:
				    pVals.append(iVal)

				    if iVal != self.pwmMap[fan][self.Tmp2Idx(iTemp)][i]:
					logging.error('PWM map Fan_{} temp={} [{}]: JSON {} != ctrl {}'.format(fan+1,
													       iTemp,
													       i,
													       iVal,
													       self.pwmMap[fan][self.Tmp2Idx(iTemp)][i]))
					rc = False

				else:
				    logging.error('PWM value {} out of range'.format(v))
				    f.close()
				    return False

				i = i+1

			    logging.debug('Verified PWM map Fan_{} @ temp {}: {}'.format(fan+1,
											 iTemp,
											 ', '.join(str(x) for x in pVals)))

			else:
			    logging.error('Wrong number of PWM values')
			    f.close()
			    return False

	f.close()
	return True


    def Close(self):
        logging.debug('Closing serial port')
        self.serPort.close()
        self.serPort = None


    def Tmp2Idx(self, temp):
        if temp <= self.tempMin:
            return 0

        if temp >= self.tempMax:
            return self.tempNumCoeffs - 1

        if ((temp - self.tempMin) % self.tempStep) != 0:
            logging.warning('Temp {} is not on the grid'.format(temp))

        return (temp - self.tempMin) / self.tempStep


    def Idx2Tmp(self, idx):
        if self.tempStep < 0:
            logging.error('Not initialized yet')
            return False

        if idx >= self.tempNumCoeffs or idx<0:
            logging.error('Wrong index')
            return False
        
        return self.tempMin + self.tempStep * idx

    
    def SendCommand(self, command):
        self.serPort.flush()
        self.serPort.write(command + '\n')
        logging.debug('Sent command "{}"'.format(command))

        success = False
        resp = ''
        for count in range(2):
            time.sleep(0.01)
            st, resp = self.CheckStatus()
            if st:
                resp = resp.rstrip()
                success = True
                logging.debug('Got real response: "{}"'.format(resp))
                break
            
        if success:
            if resp.startswith('E '):
                logging.warning('Error response')
            elif resp=='Ok':
                logging.debug('Success empty response')
            else:
                logging.debug('Success response with parameters')
        else:
            logging.warning('No response to our command')

        return success,resp


    # returns mode, pwmIn, temps, fans
    def ParseAsyncReport(self, report):
        logging.debug('ParseAsyncReport: {}'.format(report))
        tokens = report.split()
        numTokens = len(tokens)
        expTokens = self.numTemps + self.numFans + 2  # first token + pwm in
        if numTokens != expTokens:
            logging.error('Wrong number of tokens, expected {} received {}'.format(expTokens, numTokens))
            return None

        modeStr = tokens[0]
        mode = modeStr[1]
        
        pwmInStr = tokens[1]
        pwmIn = int(pwmInStr.split(':', 2)[1]) 
        
        tempsStr = tokens[2:2+self.numTemps]
        temps = []
        for i in range(self.numTemps):
            temps.append(int(tempsStr[i].split(':', 2)[1]))

        fansStr = tokens[2+self.numTemps:]
        fans = []
        for i in range(self.numFans):
            fans.append(int(fansStr[i].split(':', 2)[1]))

        logging.debug('Parsed mode:{}, pwmIn:{}, temps:{}, fans:{}'.format(mode, pwmIn, str(temps), str(fans)))
        return (mode, pwmIn, temps, fans)


    # clean - for one shot so that we do not use old values
    def CheckStatus(self, clean=False):
        if clean:
            self.serPort.reset_input_buffer()
            
        a = self.serPort.readline()
        a = a.rstrip()
        if a:
            if a[0] != '*': # non async report
                return (True, a)

            # async report
            return (False, a)

        return (False, '')

    # --------------------------------------------------------------------------------------
    def GetHwConfig(self):
        succ,resp = self.SendCommand('GetCfg')
        if not succ:
            return False

        for p in resp.split():
            n,v = p.split(':',1)
            if n=='Fans':
                self.numFans = int(v)

            elif n=='Temps':
                self.numTemps = int(v)

            elif n=='PWM_step':
                self.pwmStepSize = int(v)

            elif n=='PWM_coeffs':
                self.pwmNumCoeffs = int(v)

            elif n=='Temp_min':
                self.tempMin = int(v)

            elif n=='Temp_step':
                self.tempStep = int(v)

            elif n=='Temp_max':
                self.tempMax = int(v)

            elif n=='Temp_coeffs':
                self.tempNumCoeffs = int(v)

            else:
                logging.error('Unknown config parameter "{}"'.format(p))
                return False

        if (100+self.pwmStepSize-1)/self.pwmStepSize + 1 != self.pwmNumCoeffs:
            logging.warning('PWM num coeffs and step size does not match')

        if (self.tempMax - self.tempMin + self.tempStep - 1) / self.tempStep + 1 != self.tempNumCoeffs:
            logging.warning('Temp num coeffs and step size does not match')

        logging.debug('HW config: {} temps, {} fans'.format(self.numTemps, self.numFans))

        return True

    #--------------------- Temperature -------------------
    def GetTempWeights(self, fan):
        if self.numFans <= 0:
            logging.warning('No fans or not read hw config yet')
            return False

        if fan<1 or fan>self.numFans:
            logging.error('Wrong fan index {}'.format(fan))
            return False

        succ,resp = self.SendCommand('GetTempWeights F{}'.format(fan))
        if not succ:
            return False

        respSplit = resp.split()
        if respSplit[0] != 'F{}'.format(fan):
            logging.error('Unexpected fan "{}"'.format(respSplit[0]))
            return False

        if len(respSplit[1:]) != self.numTemps: 
            logging.error('Wrong numbe rof temp weights')
            return False

        n = 0
        for w in respSplit[1:]:
            self.tempWeights[fan-1][n] = float(w)
            logging.debug('Fan {}, temp weights {}: {}'.format( fan, n, float(w)))
            n = n+1

        return True

    def GetAllTempWeights(self):
        if self.numFans <= 0:
            logging.warning('No fans or not read hw config yet')
            return False

        for f in range(self.numFans):
            succ,resp = self.SendCommand('GetTempWeights F{}'.format(f+1))
            if not succ:
                return False

            respSplit = resp.split()
            if respSplit[0] != 'F{}'.format(f+1):
                logging.error('Unexpected fan "{}"'.format(respSplit[0]))
                return False

            n = 0
            for w in respSplit[1:]:
                if n>= self.numTemps:
                    logging.error('Too many temp weights')
                    return False

                self.tempWeights[f][n] = float(w)
                logging.debug('Fan {}, temp weights {}: {}'.format( f, n, float(w)))
                n = n+1

        logging.debug('Temp weights: {}'.format(str( self.tempWeights)))
        return True

    # SetTempWeights F1 0.2 0.3 0.5
    def SetTempWeights(self, fan, tWeights):
        if fan<1 or fan>self.numFans:
            logging.error('Wrong fan index {}'.format(fan))
            return False

        if len(tWeights) != self.numTemps:
            logging.error('Wrong number of temp weights (expcted {} but got {})'.format(self.numTemps, len(tWeights)))
            return False
        
        succ,resp = self.SendCommand('SetTempWeights F{} {}'.format(fan,
                                                                    ' '.join([str(i) for i in tWeights])))
        return succ

    # SaveTempWeights F1
    def SaveTempWeights(self, fan):
        succ,resp = self.SendCommand('SaveTempWeights F{}'.format(fan))
        return succ

    #--------------------- PWM -------------------
    def GetPwmFilt(self):
        succ,resp = self.SendCommand('GetPwmFilt')
        if not succ:
            return False

        respSplit = resp.split()
        if len(respSplit) != 2:
            logging.error('Wrong response format: ' + resp)
            return False
        
        self.pwmWeight  = float(respSplit[0])
        self.tempWeight = float(respSplit[1])
        logging.debug('PWM exponential filter: PWM weight is {} and temp weight is {}'.format(self.pwmWeight, self.tempWeight))
        return True

    # SetPwmFilt 0.1 0.1
    def SetPwmFilt(self, pwmWeight, tempWeight):
        succ,resp = self.SendCommand('SetPwmFilt {} {}'.format(pwmWeight, tempWeight))
        return succ

    def SavePwmFilt(self):
        succ,resp = self.SendCommand('SavePwmFilt')
        return succ



    # FAN - 1 based, realtemp
    def GetPwmMap(self, fan, temp):
        if fan<1 or fan>self.numFans:
            logging.error('Wrong fan index {}'.format(fan))
            return False

        gridTemp = self.Idx2Tmp(self.Tmp2Idx(temp))

        succ,resp = self.SendCommand('GetPwmMap F{} T:{}'.format(fan, gridTemp))
        if not succ:
            return False

        respSplit = resp.split()
        if(len(respSplit)) != self.pwmNumCoeffs + 2:
            logging.error('Received wrong number of params')
            return False
                
        if respSplit[0] != 'F{}'.format(fan):
            logging.error('Expected fan F{} but got {}'.format(f+1, respSplit[0]))
            return False

        if respSplit[1] != 'T:{}'.format(gridTemp):
            logging.error('Expected temp. T:{} but got {}'.format(gridTemp, respSplit[1]))
            return False

        tempIdx = self.Tmp2Idx(gridTemp)
        for p in range(self.pwmNumCoeffs):
            self.pwmMap[fan-1][tempIdx][p] = int(respSplit[p+2])

        logging.debug('Got PWM coeffs for F{} T:{} : {}'.format(fan, gridTemp, self.pwmMap[fan-1][tempIdx]))

        return True

    def GetPwmMapAll(self):
        if self.numFans <= 0:
            logging.warning('No fans or not read hw config yet')
            return False

        if self.numTemps <= 0:
            logging.warning('No temperature sensors or not read hw config yet')
            return False

        for f in range(self.numFans):
            for t in range(self.tempNumCoeffs):
                realTmp = self.Idx2Tmp(t)
                succ,resp = self.SendCommand('GetPwmMap F{} T:{}'.format(f+1, realTmp))
                if not succ:
                    return False

                # TBD
                respSplit = resp.split()
                if(len(respSplit)) != self.pwmNumCoeffs + 2:
                    logging.error('Received wrong number of params')
                    return False
                
                if respSplit[0] != 'F{}'.format(f+1):
                    logging.error('Expected fan F{} but got {}'.format(f+1, respSplit[0]))
                    return False

                if respSplit[1] != 'T:{}'.format(realTmp):
                    logging.error('Expected temp. T:{} but got {}'.format(realTmp, respSplit[1]))
                    return False

                for p in range(self.pwmNumCoeffs):
                    self.pwmMap[f][t][p] = int(respSplit[p+2])

                logging.debug('Got PWM coeffs for F{} T:{} : {}'.format(f+1, realTmp, self.pwmMap[f][t]))

        return True

    #SetPwmMap F1 T:20 0 10 15 20 25 30 35 40 45 50 55 60 65 70 75 80 85 90 95 100
    def SetPwmMap(self, fan, temp, pwmVals):
        if fan<1 or fan>self.numFans:
            logging.warning('Wrong fan number {} (must be between 1 and {})'.format(fan, self.numFans))
            return False

        gridTemp = self.Idx2Tmp(self.Tmp2Idx(temp))

        if len(pwmVals) != self.pwmNumCoeffs:
            logging.warning('Wrong number of PWM values - expected {} but got {}'.format(self.pwmNumCoeffs, len(pwmVals)))
            return False

        succ,resp = self.SendCommand('SetPwmMap F{} T:{} {}'.format(fan,
                                                                    gridTemp,
                                                                    ' '.join([str(i) for i in pwmVals])))
        return succ
        


    # SavePwmMap F1 T:20
    def SavePwmMap(self, fan, temp):
        if fan<1 or fan>self.numFans:
            logging.warning('Wrong fan number {} (must be between 1 and {})'.format(fan, self.numFans))
            return False

        gridTemp = self.Idx2Tmp(self.Tmp2Idx(temp))

        succ,resp = self.SendCommand('SavePwmMap F{} T:{}'.format(fan, gridTemp))
        return succ

    
    #--------------------- Opmode -------------------

    # ModeManual F1:20 F2:30
    def ModeManual(self, fVals):
        if len(fVals) != self.numFans:
            logging.warning('Wrong number of values - expected {} but got {}'.format(self.numFans, len(fVals)))
            return False

        f = []
        fIdx = 1
        for i in fVals:
            if i<0 or i>100:
                logging.warning('Fan PWM value must be between 0 and 100, got {}'.format(i))
                return False
            f.append('F{}:{}'.format(fIdx, i))
            fIdx = fIdx + 1

        succ,resp = self.SendCommand('ModeManual {}'.format(' '.join(f)))
        return succ


    def ModeAuto(self):
        succ,resp = self.SendCommand('ModeAuto')
        return succ

    # ModeFailsafe
    def ModeFailsafe(self):
        succ,resp = self.SendCommand('ModeFailsafe')
        return succ


    #----------------- Verify --------------------
    def AverageTemps(self, index, temps):
        tSum   = 0.0
        wSum   = 0.0
        result = 0

        for a in range(self.numTemps):
            tSum = tSum + self.tempWeights[index][a] * temps[a]
            wSum = wSum + self.tempWeights[index][a]

        result = int(tSum / wSum)

        if result < self.tempMin:
            result = self.tempMin
        elif result > self.tempMax:
            result = self.tempMax

        logging.debug('Average temp for fan {} is {}'.format(index, result))
        return result;


    def InterpolatePwm(self, index, pwmIn, temp):
        temp = temp - self.tempMin

        idxTmp1 = int(temp / self.tempStep)
        distTmp = temp % self.tempStep
        idxTmp2 = idxTmp1

        if distTmp != 0:
            idxTmp2 = idxTmp2+1

        #print idxTmp1, distTmp, idxTmp2


        idxPwm1 = int(pwmIn / self.pwmStepSize)
        distPwm = pwmIn % self.pwmStepSize
        idxPwm2 = idxPwm1

        if distPwm != 0:
            idxPwm2 = idxPwm2+1

        #print idxPwm1, distPwm, idxPwm2

        pwmVal1 = ( float(self.pwmMap[index][idxTmp1][idxPwm1]) * float(self.pwmStepSize-distPwm) \
                    + \
                    float(self.pwmMap[index][idxTmp1][idxPwm2]) * float(distPwm) ) \
                    / float(self.pwmStepSize)
        #print pwmVal1

        pwmVal2 = ( float(self.pwmMap[index][idxTmp2][idxPwm1]) * float(self.pwmStepSize-distPwm) \
                    + \
                    float(self.pwmMap[index][idxTmp2][idxPwm2]) * float(distPwm) ) \
                    / float(self.pwmStepSize)

        #print pwmVal1

        pwmOut = (pwmVal1 * float(self.tempStep-distTmp) + pwmVal2 * float(distTmp)) / float(self.tempStep)

        logging.debug('Interpolated PWM out for fan {} is {}'.format(index, pwmOut))
        return int(pwmOut+0.5)


    
    def VerifyControl(self, iterations, stopOnError=False):
        success = True
        for a in range(iterations):
            status, resp = self.CheckStatus()
            if status:
                # should not happen, there should be only async msgs
                logging.warning('Standard response received ("{}") but we have not sent a request'.format(resp))
                success = False

            elif resp:
                logging.debug('Received response {} ({}/{})'.format(resp, a+1, iterations))
                (mode, pwmIn, temps, fans) = self.ParseAsyncReport(resp)

                if mode=='F':   # copy input to output
                    for i in range(self.numFans):
                        if fans[i] != pwmIn:
                            logging.error('In failsafe mode fan {} should be the same as PWM in {} but is {}'.format(i+1, pwmIn, fans[i]))
                            success = False

                elif mode=='A': # compute the magic
                    for i in range(self.numFans):
                        temp = self.AverageTemps(i, temps)
                        pwmOut = self.InterpolatePwm(i, pwmIn, temp)
                        if fans[i] != pwmOut:
                            logging.error('In auto mode fan {} should be {} but is {}'.format(i+1, pwmOut, fans[i]))
                            success = False
                        else:
                            logging.debug('In auto mode fan {} is correctly {}'.format(i+1, pwmOut))

                elif mode=='M': # nothing to test here
                    pass

                else:
                    logging.error('Unknown mode {}, this should NEVER happen'.format(mode))
                
            if stopOnError and not success:
                logging.debug('Breaking after the first error')
                break

        if success:
            logging.info('The test was successful!')
        else:
            logging.error('The test has failed!')

        return success
