#!/usr/bin/env python2

import logging
import sys
from optparse import OptionParser
from scipy import interpolate
import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d import axes3d
import numpy as np

# see https://stackoverflow.com/questions/31543775/how-to-perform-cubic-spline-interpolation-in-python
# see https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.interp2d.html#scipy.interpolate.interp2d
def Interpolate2D():
    pwmIn =  [ 0, 6, 12, 25, 30, 50, 60]
    pwmOut = [ 0, 6, 10, 12, 16, 29, 55]

    myMax = 55
    pwmFunc = interpolate.interp1d(pwmIn, pwmOut, kind='cubic', bounds_error=False, assume_sorted=True, fill_value=myMax)
    
    xnew = np.arange(0, 100, 5)
    ynew = pwmFunc(xnew)
    plt.plot(pwmIn, pwmOut, 'o', xnew, ynew, '-')
    plt.xlabel('PWM in [%]')
    plt.ylabel('PWM out [%]')
    plt.show()

def Interpolate3D():
    tempIn = []
    pwmIn  = []
    pwmOut = []
    
    tempIn += [ 25, 25, 25, 25, 25, 25, 25 ]
    pwmIn  += [  0,  6, 12, 25, 30, 50, 60 ]
    pwmOut += [  0,  6, 10, 12, 16, 29, 55 ]
    
    tempIn += [ 30, 30, 30, 30, 30, 30, 30 ]
    pwmIn  += [  0,  6, 12, 25, 30, 50, 60 ]
    pwmOut += [  0,  6, 11, 13, 17, 29, 55 ]

    tempIn += [ 35, 35, 35, 35, 35, 35, 35 ]
    pwmIn  += [  0,  6, 12, 25, 30, 50, 60 ]
    pwmOut += [  0,  6, 12, 14, 19, 30, 55 ]

    tempIn += [ 40, 40, 40, 40, 40, 40, 40 ]
    pwmIn  += [  0,  6, 12, 25, 30, 50, 60 ]
    pwmOut += [  0,  6, 15, 17, 25, 40, 55 ]

    tempIn += [ 45, 45, 45, 45, 45, 45, 45 ]
    pwmIn  += [  0,  6, 12, 25, 30, 50, 60 ]
    pwmOut += [  0,  6, 16, 18, 27, 41, 55 ]

    #tempIn = np.asarray(tempIn)
    #pwmIn = np.asarray(pwmIn)
    #pwmOut = np.asarray(pwmOut)

    tempIn = np.array(tempIn)
    pwmIn  = np.array(pwmIn)
    pwmOut = np.array(pwmOut)

    print '--------------'
    print tempIn
    sds = np.atleast_1d(tempIn)
    print sds
    print sds.ndim
    print '--------------'
    print pwmIn
    sds = np.atleast_1d(pwmIn)
    print sds
    print sds.ndim
    print '--------------'
    
    print pwmOut

    #5*7 = 35
    print len(tempIn)
    print len(pwmIn)
    print len(pwmOut)

    myMax=60
    interpFunc = interpolate.interp2d(pwmIn, tempIn, pwmOut, kind='cubic', bounds_error=False, fill_value=myMax)

    #inp = interpolate.SmoothBivariateSpline(pwmIn, tempIn, pwmOut)
    #interpFunc = inp.ev

    print '-----arrange---------'
    X = np.arange( 0, 101, 5)  #21
    Y = np.arange(25,  46, 5)  #5
    Z = interpFunc(X, Y)


    # Plot the surface.
    fig = plt.figure()
    #ax = fig.gca(projection='3d')
    ax = fig.add_subplot(111, projection='3d')
    #surf = ax.plot_surface(X, Y, Z, cmap=cm.coolwarm, linewidth=0, antialiased=False)

    X2, Y2 = np.meshgrid(X, Y)
    surf = ax.plot_wireframe(X2, Y2, Z)
    #surf = ax.plot_surface(X2, Y2, Z)

    # Customize the z axis.
    #ax.set_zlim(0, 100)
    #ax.zaxis.set_major_locator(LinearLocator(10))
    #ax.zaxis.set_major_formatter(FormatStrFormatter('%.02f'))

    ## Add a color bar which maps values to colors.
    #fig.colorbar(surf, shrink=0.5, aspect=5)

    plt.show()


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
    
    parser.add_option('-r', '--rest-wait',
                      dest    = 'resetWait',
                      metavar = 'RESETWAIT',
                      help    = 'Wait for controller reset.',
                      default = False,
                      action  = 'store_true'
                      )

    parser.add_option('-f', '--log-file',
                      dest    = 'logFile',
                      metavar = 'LOGFILE',
                      help    = 'Name of the output CSV log file',
                      default = 'data.csv'
                      )

    (options, args) = parser.parse_args()

    if not options:
        parser.print_help()
        sys.exit(1)

    if options.debugOn:
        rootlogger.setLevel(logging.DEBUG)
        logging.info("Verbose ON")

    Interpolate2D()
    #Interpolate3D()


#---------------------------
if __name__ == "__main__":
    main()        
