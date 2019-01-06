#!/usr/bin/env python2

from __future__ import print_function

#import subprocess
import json
import logging
from optparse import OptionParser









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

    (options, args) = parser.parse_args()

    if not options:
        parser.print_help()
        sys.exit(1)

    if options.debugOn:
        rootlogger.setLevel(logging.DEBUG)
        logging.info("Verbose ON")

    fileName = '/_DATA_/Rest/devel/SVN/ProliantFanControl/ProliantFanControlClient/ML350.json'

    f = open(fileName, 'r')
    data = json.load(f)
    f.close()

    pMap = data.get('PWM_map')
    numCoeffs = int(data.get('PWM_coeffs'))
    pwmStep = int(data.get('PWM_step'))
    
    for fan, fMap in pMap.iteritems():
        outFileName = 'PwmMap_{}.csv'.format(fan)
        logging.debug('Writing {}'.format(outFileName))
        oF = open(outFileName, 'w')

        temps = sorted(fMap.keys())

        print('Pwm_in, ' + ', '.join(str(x)+'C' for x in temps), file=oF)
        for p in range(numCoeffs):
            print(str(p*pwmStep) + ', ' + ', '.join(str(fMap[x][p]) for x in temps), file=oF)

        oF.close()

        


#---------------------------
if __name__ == "__main__":
    main()        
