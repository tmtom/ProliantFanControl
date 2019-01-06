#!/usr/bin/env python2

# https://pythonhosted.org/pyserial/pyserial.html

import logging
import logging.handlers
import sys
import datetime

from optparse import OptionParser

import ProliantFanClient

def main():
    logging.basicConfig(level=logging.INFO,
                        format='%(asctime)s %(levelname)s %(message)s'
                        )

    rootLogger = logging.getLogger()

    logging.addLevelName( logging.INFO, "\033[93m%s\033[1;0m" % logging.getLevelName(logging.INFO))
    logging.addLevelName( logging.WARNING, "\033[1;31m%s\033[1;0m" % logging.getLevelName(logging.WARNING))
    logging.addLevelName( logging.ERROR, "\033[1;41m%s\033[1;0m" % logging.getLevelName(logging.ERROR))

    parser = OptionParser(version='0.1')

    parser.add_option('-d', '--device',
                      dest    = 'device',
                      metavar = 'DEVICE',
                      default = '/dev/ttyUSB0',
                      help    = 'ProliantFanControl serial device'
                      )
    
    parser.add_option('-r', '--rest-wait',
                      dest    = 'resetWait',
                      metavar = 'RESETWAIT',
                      help    = 'Wait for controller reset.',
                      default = False,
                      action  = 'store_true'
                      )

    parser.add_option('-p', '--pwm-filter',
                      dest    = 'pwmFilter',
                      metavar = 'PWMFILTER',
                      default = 0.1,
                      type    = 'float',
                      help    = 'PWM exp. filter weight'
                      )

    parser.add_option('-t', '--temp-filter',
                      dest    = 'tempFilter',
                      metavar = 'TEMPFILTER',
                      default = 0.1,
                      type    = 'float',
                      help    = 'Temp. exp. filter weight'
                      )

    parser.add_option('-f', '--filename',
                      dest    = 'fileName',
                      metavar = 'FILENAME',
                      help    = 'Name of the input or output file (depending on the command)'
                      )

    parser.add_option('-F', '--fan-number',
                      dest    = 'fanNumber',
                      metavar = 'FANNUMBER',
                      default = 1,
                      type    = 'int',
                      help    = 'One based fan index'
                      )

    parser.add_option('-T', '--temperature',
                      dest    = 'temperature',
                      metavar = 'TEMPERATURE',
                      default = 25,
                      type    = 'int',
                      help    = 'Temperature in C. Ideally shoudl be on the "grid" (temp min + n * temp step)'
                      )

    parser.add_option('-s', '--temp-weights',
                      dest    = 'tempWeights',
                      metavar = 'TEMPWEIGHTS',
                      default = None,
                      help    = 'Temperature sensor weights (floats between 0 and 1) for the given fan. Pass the array as a string - e.g. \'-s "0.1 0.3 0.6"\'. The sum should be exactly 1.0.'
                      )

    parser.add_option('-P', '--pwm-values',
                      dest    = 'pwmMap',
                      metavar = 'PWMMAP',
                      default = None,
                      help    = 'List of PWM values (duty cycle - integers between 0 an 100). Pass the array as a string - e.g. \'-P "0 10 20 50 100"\'.'
                      )


    optCommands = [ 'Dump', 'Monitor',
                    'GetFilter', 'SetFilter', 'SaveFilter',
                    'GetTempWeights', 'SetTempWeights', 'SaveTempWeights',
                    'GetPwmMap', 'SetPwmMap', 'SavePwmMap',
		    'DumpJson', 'LoadJson', 'LoadCommitJson', 'VerifyJson',
		    'ModeManual', 'ModeAuto', 'ModeFailsafe' ]

    parser.add_option('-c', '--command',
                      dest    = 'cmds',
                      metavar = 'CMDS',
                      help    = 'Command / what to be done. Supported command are: ' + ', '.join(optCommands) + '. Can be repeated multiple times.',
                      default = [ ],
                      choices = optCommands,
                      action  = 'append'
                      )

    parser.add_option('-v', '--verbose',
                      dest    = 'debugOn',
                      metavar = 'VERBOSE',
                      help    = 'Verbose / debug info.',
                      default = False,
                      action  = 'store_true'
                      )

    (options, args) = parser.parse_args()


    if(options.debugOn):
        rootLogger.setLevel(logging.DEBUG)        
        logging.info("Verbose ON")
    else:
        rootLogger.setLevel(logging.INFO)


    ctrl = ProliantFanClient.FanController(comPort=options.device, waitForReset=options.resetWait)
    if not ctrl.Open():
        sys.exit(1)

    rc = 0
    for cmd in options.cmds:
        #-------- Dump --------
        if(cmd == 'Dump'):
            if ctrl.ReadAll():
                if not ctrl.Dump(options.fileName):
                    rc = 3
            else:
                ctrl.Close()
                sys.exit(2)
    
   
        #-------- Filter --------
        elif(cmd == 'GetFilter'):
            if not ctrl.GetPwmFilt():
                logging.error('GetPwmFilt failed')
                rc = 5
            else:
                print 'GetFilter OK'
                print 'Filt_PWM:    {}'.format(ctrl.pwmWeight)
                print 'Filt_temp:   {}'.format(ctrl.tempWeight)
    
        elif(cmd == 'SetFilter'):
            if not ctrl.SetPwmFilt(options.pwmFilter, options.tempFilter):
                logging.error('SetPwmFilt failed')
                rc = 6
            else:
                print 'SetFilter OK'
    
        elif(cmd == 'SaveFilter'):
            if not ctrl.SavePwmFilt():
                logging.error('SavePwmFilt failed')
                rc = 7
            else:
                print 'SaveFilter OK'
    
        #-------- TempWeights --------
        elif(cmd == 'GetTempWeights'):
            if not ctrl.GetTempWeights(options.fanNumber):
                logging.error('GetTempWeights failed')
                rc = 8
            else:
                print 'GetTempWeights OK'
                print 'TempWeights:'
                print '  Fan_{}: '.format(options.fanNumber) + ', '.join(str(x) for x in ctrl.tempWeights[options.fanNumber - 1])

        elif(cmd == 'SetTempWeights'):
            if not options.tempWeights:
                logging.error('No temp weights provided')
                sys.exit(-1)

            tempWeights = []
            for w in options.tempWeights.split():
                tempWeights.append(float(w))

            if not ctrl.SetTempWeights(options.fanNumber, tempWeights):
                logging.error('SetTempWeights failed')
                rc = 9
            else:
                print 'SetTempWeights OK'

        elif(cmd == 'SaveTempWeights'):
            if not ctrl.SaveTempWeights(options.fanNumber):
                logging.error('SaveTempWeights failed')
                rc = 10
            else:
                print 'SaveTempWeights OK'

        #-------- PwmMap --------
        elif(cmd == 'GetPwmMap'):

            tempIdx = ctrl.Tmp2Idx(options.temperature)
            gridTemp = ctrl.Idx2Tmp(tempIdx)
            if options.temperature != gridTemp:
                logging.warning('Temperature {} is not in the grid, correcting to {}'.format(options.temperature, gridTemp))

            if not ctrl.GetPwmMap(options.fanNumber, gridTemp):
                logging.error('GetPwmMap failed')
                rc = 11
            else:
                print 'GetPwmMap OK'
                print 'TempWeights:'
                print '  Fan_{}: T={}: {}'.format(options.fanNumber,
                                                  gridTemp,
                                                  ', '.join(str(x) for x in ctrl.pwmMap[options.fanNumber-1][tempIdx]))

        elif(cmd == 'SetPwmMap'):
            tempIdx = ctrl.Tmp2Idx(options.temperature)
            gridTemp = ctrl.Idx2Tmp(tempIdx)
            if options.temperature != gridTemp:
                logging.warning('Temperature {} is not in the grid, correcting to {}'.format(options.temperature, gridTemp))

            if not options.pwmMap:
                logging.error('No PWM map values provided')
                sys.exit(-1)

            pwmMap = []
            for p in options.pwmMap.split():
                pwmMap.append(int(p))

            if not ctrl.SetPwmMap(options.fanNumber, gridTemp, pwmMap):
                logging.error('SetPwmMap failed')
                rc = 12
            else:
                print 'SetPwmMap OK'


        elif(cmd == 'SavePwmMap'):

            tempIdx = ctrl.Tmp2Idx(options.temperature)
            gridTemp = ctrl.Idx2Tmp(tempIdx)
            if options.temperature != gridTemp:
                logging.warning('Temperature {} is not in the grid, correcting to {}'.format(options.temperature, gridTemp))

            if not ctrl.SavePwmMap(options.fanNumber, gridTemp):
                logging.error('SavePwmMap failed')
                rc = 11
            else:
                print 'SavePwmMap OK'


        #-------- Json --------
        elif(cmd == 'DumpJson'):
            if ctrl.ReadAll():
                if not ctrl.DumpJson(options.fileName):
                    logging.error('DumpJson failed')
                    rc = 4
                else:
                    print 'DumpJson OK'

            else:
                ctrl.Close()
                sys.exit(2)

        elif(cmd == 'LoadJson'):
            if ctrl.ReadAll():
		if not options.fileName:
		    logging.error('No file provided')
		    ctrl.Close()
		    sys.exit(-1)

                if not ctrl.LoadJson(options.fileName, save=False):
                    rc = 12
                else:
                    print 'LoadJson OK'

            else:
                ctrl.Close()
                sys.exit(2)

        elif(cmd == 'LoadCommitJson'):
            if ctrl.ReadAll():
		if not options.fileName:
		    logging.error('No file provided')
		    ctrl.Close()
		    sys.exit(-1)

                if not ctrl.LoadJson(options.fileName, save=True):
                    rc = 12
                else:
                    print 'LoadCommitJson OK'

            else:
                ctrl.Close()
                sys.exit(2)

        elif(cmd == 'VerifyJson'):
            if ctrl.ReadAll():
		if not options.fileName:
		    logging.error('No file provided')
		    ctrl.Close()
		    sys.exit(-1)

                if not ctrl.VerifyJson(options.fileName):
                    rc = 13
                else:
                    print 'VerifyJson OK'

            else:
                ctrl.Close()
                sys.exit(2)


	#------- Opmodes -------
        elif(cmd == 'ModeManual'):
            if not options.pwmMap:
                logging.error('No PWM values provided')
                sys.exit(-1)

            pwmList = []
            for p in options.pwmMap.split():
                pwmList.append(int(p))
	    
	    if not ctrl.ModeManual(pwmList):
                logging.error('ModeManual failed')
                rc = 14
            else:
                print 'ModeManual OK'

        elif(cmd == 'ModeAuto'):
	    if not ctrl.ModeAuto():
                logging.error('ModeAuto failed')
                rc = 15
            else:
                print 'ModeAuto OK'

        elif(cmd == 'ModeFailsafe'):
	    if not ctrl.ModeFailsafe():
                logging.error('ModeFailsafe failed')
                rc = 16
            else:
                print 'ModeFailsafe OK'


	#------- Monitor -------
        elif(cmd == 'Monitor'):
	    while True:
		st, a = ctrl.CheckStatus()
		if st:
		    print a
		elif a:
		    print str(datetime.datetime.now())+'  '+a




        else:
            logging.error('Unsupported command "{}"'.format(cmd))
            rc = -1

        if rc != 0:
            break

        print


    ctrl.Close()
    sys.exit(rc)




if __name__ == "__main__":
    main()




