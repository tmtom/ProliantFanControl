#!/usr/bin/env python2

import subprocess
import logging
import sys
import socket
from optparse import OptionParser
import time
import psutil

import ProliantFanClient

''' need to be run as root because of IPMI...
Run "hddtemp -d"
'''

# hdparm -S 0  /dev/sde
# sdparm --clear STANDBY -6 /dev/sde
# hddtemp -w -d /dev/sd[a-f]
# killall hddtemp]


def ReadHdd():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('localhost', 7634))
    time.sleep(0.5)
    data = s.recv(4096)
    s.close()

    logging.debug('Received from hddtemp: "{}"'.format(data))

    sData = data.split('|')

    # it starts with |
    numDisks = (len(sData) - 1) / 5
    logging.debug('We have {} disks'.format(numDisks))

    allDisks = {}
    for diskIndex in range(numDisks):
        baseIndex = 5 * diskIndex + 1
        dev = sData[baseIndex]
        desc = sData[baseIndex+1]
        temp = sData[baseIndex+2]
        unit = sData[baseIndex+3]
        logging.debug('Parsed device num {}: {} ({}) temp {} units {}'.format(diskIndex, dev, desc, temp, unit))
        dName = '{} ({})'.format(dev, desc)
        allDisks[dName] = temp

    return allDisks



def ReadIpmi():
    logging.debug('Reading IPMI sensors')

    tempData = {}
    fanData = {}

    probeData = subprocess.check_output('ipmitool sensor', shell=True)

    for line in probeData.splitlines():
        fields = line.split('|')
        if len(fields) != 10:
            logging.warning('Wrong line format "{}"'.format(line))

        else:
            params = []
            for param in fields:
                params.append(param.strip())

            if params[2] == 'degrees C':
                tempData[params[0]] = float(params[1])
                
            elif params[2] == 'percent' and 'fan' in params[0].lower():
                fanData[params[0]] = float(params[1])

    logging.debug('Got {} temp sensors and {} fans'.format(len(tempData), len(fanData)))

    return (tempData, fanData)
                      

def CreateGpScript(gFileName, numColumns, outFile, dataFile, smooth=False):
    logging.info('Writing GNUPlot script to "{}"'.format(gFileName))

    if smooth:
        graphType = 'smooth csplines'
    else:
        graphType = 'with lines'
        
    with open(gFileName, 'w') as gFile:
        gFile.write('set terminal pdf size 29.7cm,21.0cm\n') # create A4
        gFile.write('set output "{}"\n'.format(outFile))
        gFile.write('set datafile separator \',\'\n')
        gFile.write('set key autotitle columnhead\n')
        gFile.write('plot for [i=2:{}] \'{}\' using 1:i {}\n'.format(numColumns, dataFile, graphType))


def main():
    logging.basicConfig(level=logging.INFO,
                        format='%(asctime)s %(levelname)s {%(filename)s:%(lineno)d} %(message)s')

    rootlogger = logging.getLogger()

    logging.addLevelName(logging.INFO,
                         "\033[93m%s\033[1;0m" %(logging.getLevelName(logging.INFO)))
    logging.addLevelName(logging.WARNING,
                         "\033[1;31m%s\033[1;0m" %(logging.getLevelName(logging.WARNING)))
    logging.addLevelName(logging.ERROR,
                         "\033[1;41m%s\033[1;0m" %(logging.getLevelName(logging.ERROR)))


    parser = OptionParser(version='0.1')

    parser.add_option('-v', '--verbose',
                      dest    = 'debugOn',
                      metavar = 'VERBOSE',
                      help    = 'Verbose / debug info.',
                      default = False,
                      action  = 'store_true'
                     )

    parser.add_option('-d', '--device',
                      dest    = 'device',
                      metavar = 'DEVICE',
                      default = '/dev/ttyUSB0',
                      help    = 'ProliantFanControl serial device'
                      )
    
    parser.add_option('-r', '--reset-wait',
                      dest    = 'resetWait',
                      metavar = 'RESETWAIT',
                      help    = 'Wait for controller reset.',
                      default = False,
                      action  = 'store_true'
                      )

    parser.add_option('-f', '--log-file',
                      dest    = 'logFile',
                      metavar = 'LOGFILE',
                      help    = 'Name of the output CSV log file'
                      )

    parser.add_option('-n', '--no-pfc',
                      dest    = 'noPfc',
                      metavar = 'NOPFC',
                      help    = 'Do not use ProliantFanControl feature',
                      default = False,
                      action  = 'store_true'
                      )

    (options, args) = parser.parse_args()

    if not options:
        parser.print_help()
        sys.exit(1)

    if options.debugOn:
        rootlogger.setLevel(logging.DEBUG)
        logging.info("Verbose ON")

    usePfc = True
    if options.noPfc:
        usePfc = False
        logging.info('Will not use ProliantFanControl')


    baseName  = 'PfcMonitor'
    timestamp = time.strftime('%Y%m%d-%H%M%S')
    timeBaseName = '{}_{}'.format(baseName, timestamp)
    if not options.logFile:
        options.logFile = timeBaseName + '.csv'

    gFileName = timeBaseName + '.gp'
    outFile   = timeBaseName + '.pdf'

    ctrl = None
    if usePfc:
        ctrl = ProliantFanClient.FanController(comPort=options.device, waitForReset=options.resetWait)
        if not ctrl.Open():
            sys.exit(1)


    start = True

    oFile = None
    ipmiTempKeys = []
    ipmiFanKeys = []
    diskKeys = []

    # make first dummy read as recommended
    cpuUse = psutil.cpu_percent()
    
    while True:
        try:

            if usePfc:
                status, response = ctrl.CheckStatus(True)
                if status:
                    logging.warning('Received real response (not an astync report): {}'.format(response))
                    # try again??
                elif response:
                    (pfcMode, pfcPwmIn, pfcTemps, pfcFans) = ctrl.ParseAsyncReport(response)
                else:
                    logging.warning('No async report??')

            
            cpuUse = psutil.cpu_percent()
            tData, fData = ReadIpmi()
            dData = ReadHdd()
    
            if start:
                csvHeader = 'Time'
                
                if usePfc:
                    # add procssing headers from PFC
                    csvHeader = csvHeader + ', PwmIn'
                    
                    for i in range(len(pfcTemps)):
                        csvHeader = csvHeader + ', T_{}'.format(i)

                    for i in range(len(pfcFans)):
                        csvHeader = csvHeader + ', Fan{}_out'.format(i)
                    
                ipmiFanKeys  = sorted(fData.keys())
                ipmiTempKeys = sorted(tData.keys())
                diskKeys     = sorted(dData.keys())

                if usePfc:
                    logging.info('Will collect {} IPMI fans, {} IPMI temps, {} HDD temps, {} PFC temps, {} PFC fans'.format(
                        len(ipmiFanKeys),
                        len(ipmiTempKeys),
                        len(diskKeys),
                        len(pfcTemps),
                        len(pfcFans)))
                else:
                    logging.info('Will collect {} IPMI fans, {} IPMI temps, {} HDD temps'.format(
                        len(ipmiFanKeys),
                        len(ipmiTempKeys),
                        len(diskKeys)))
                    
                if usePfc:
                    numColumns = 1 + 1 + len(pfcTemps) + len(pfcFans) + len(ipmiFanKeys) + len(ipmiTempKeys) + len(diskKeys)
                else:
                    numColumns = 1 + len(ipmiFanKeys) + len(ipmiTempKeys) + len(diskKeys)

                CreateGpScript(gFileName, numColumns, outFile, options.logFile)

                csvHeader = csvHeader + ', CPU_use'

                csvHeader = csvHeader + ', ' + ', '.join(ipmiFanKeys)
                csvHeader = csvHeader + ', ' + ', '.join(ipmiTempKeys)
                csvHeader = csvHeader + ', ' + ', '.join(diskKeys)
                
                oFile = open(options.logFile, 'w')
                logging.info('Opening "{}" for logging data'.format(options.logFile))

                logging.debug(csvHeader)
                oFile.write(csvHeader + '\n')
                start = False
                startTime = time.time()
                currTime = 0.0

            else:
                currTime = time.time() - startTime
    
            csvDataLine = '{:.2f}'.format(currTime)

            if usePfc:
                csvDataLine = csvDataLine + ', {}'.format(pfcPwmIn)
                
                for i in pfcTemps:
                    csvDataLine = csvDataLine + ', {}'.format(i)

                for i in pfcFans:
                    csvDataLine = csvDataLine + ', {}'.format(i)

            csvDataLine = csvDataLine + ', {}'.format(cpuUse)

            csvDataLine = csvDataLine + ', ' + ', '.join(str(fData[x]) for x in ipmiFanKeys)
            csvDataLine = csvDataLine + ', ' + ', '.join(str(tData[x]) for x in ipmiTempKeys)
            csvDataLine = csvDataLine + ', ' + ', '.join(str(dData[x]) for x in diskKeys)
            
            logging.debug(csvDataLine)

            maxIpmiTempKey = max(tData, key=tData.get)
            if usePfc:
                maxPfcTemp = max(pfcTemps)
                maxPfcTempIndex = pfcTemps.index(maxPfcTemp)
                logging.info('Max temps: IPMI {} @ {}, PFC {} @ T_{}'.format(tData[maxIpmiTempKey], maxIpmiTempKey, maxPfcTemp, maxPfcTempIndex))
            else:
                logging.info('Max temps: IPMI {} @ {}'.format(tData[maxIpmiTempKey], maxIpmiTempKey))
            
            oFile.write(csvDataLine + '\n')
            oFile.flush()
    
            time.sleep(3)

        except KeyboardInterrupt:
            logging.info('Keyboard interrup - closing')
            break
    

    if usePfc:
        ctrl.Close()
        
    oFile.close()

    logging.info('Now you can run "gnuplot {}" to create graph {}'.format(gFileName, outFile))
    
    sys.exit(0)


#---------------------------
if __name__ == "__main__":
    main()        
