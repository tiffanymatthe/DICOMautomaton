import numpy as np  # importing numpy module for efficiently executing numerical operations
import matplotlib.pyplot as plt  # importing the pyplot from the matplotlib library
from scipy import signal
import sys, getopt

def main(argv):
    cfreqPercent = 0.18
    try:
        opts, args = getopt.getopt(argv,"h:c:",["cfreq="])
    except getopt.GetoptError:
        print('sanitizeInputData.py -h <help> -c <cutoff percentage (decimagl value from 0 to 1)>')
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print('sanitizeInputData.py -h <help> -c <cutoff percentage (decimagl value from 0 to 1)>')
            sys.exit()
        elif opt in ("-c", "--cfreq"):
            cfreqPercent = arg
    print ('Percentage cutoff of Nyquist frequency: ', cfreqPercent)
    sanitizeData(cfreqPercent)

def sanitizeData(cfreqPercent):
    with open('data/raw/C_000009.txt') as f:
        lines = f.readlines()
        c_t = [float(line.split()[0]) for line in lines]
        c_i = [float(line.split()[2]) for line in lines]

    with open('data/raw/AIF_000000.txt') as f:
        lines = f.readlines()
        a_t = [float(line.split()[0]) for line in lines]
        a_i = [float(line.split()[2]) for line in lines]

    with open('data/raw/VIF_000001.txt') as f:
        lines = f.readlines()
        v_t = [float(line.split()[0]) for line in lines]
        v_i = [float(line.split()[2]) for line in lines]

    # - - - # We load the data in the mat format but this code will work for any sort of time series.# - - - #
    cT = np.array(c_t)
    cI = np.array(c_i)
    aT = np.array(a_t)
    aI = np.array(a_i)
    vT = np.array(v_t)
    vI = np.array(v_i)

    #  Visualizing the original and the Filtered Time Series
    fig = plt.figure()
    ax = fig.add_subplot(1, 1, 1)
    ax.plot(c_t, c_i, 'k-', lw=0.5)

    # # Filtering of the time series
    # fs = 1/1.2  # Sampling period is 1.2s so f = 1/T

    # nyquist = fs / 2  # 0.5 times the sampling frequency
    # cutoff = 0.18  # fraction of nyquist frequency
    # # b, a = signal.butter(5, cutoff, btype='lowpass')  # low pass filter
    maxAllowableFreq = findMaxAllowableFrequency(cT)
    b, a = signal.butter(5, cfreqPercent*maxAllowableFreq, btype='lowpass')  # low pass filter

    cIfilt = signal.filtfilt(b, a, cI)
    cIfilt = np.array(cIfilt)
    cIfilt = cIfilt.transpose()

    aIfilt = signal.filtfilt(b, a, aI)
    aIfilt = np.array(aIfilt)
    aIfilt = aIfilt.transpose()

    vIfilt = signal.filtfilt(b, a, vI)
    vIfilt = np.array(vIfilt)
    vIfilt = vIfilt.transpose()

    # write filtered data back into a txt file
    zeroArr = np.zeros((cT.shape[0],), dtype=int)
    Cdata = np.column_stack((cT, zeroArr, cIfilt, zeroArr))
    Adata = np.column_stack((aT, zeroArr, aIfilt, zeroArr))
    Vdata = np.column_stack((vT, zeroArr, vIfilt, zeroArr))

    with open('data/input/sanitized_c.txt', 'w') as txt_file:
        txt_file.write('0.0 0 0.0 0\n')
        for row in Cdata:
            line = ' '.join(str(v) for v in row)
            # works with any number of elements in a line
            txt_file.write(line + "\n")

    with open('data/input/sanitized_aif.txt', 'w') as txt_file:
        txt_file.write('0.0 0 0.0 0\n')
        for row in Adata:
            line = ' '.join(str(v) for v in row)
            # works with any number of elements in a line
            txt_file.write(line + "\n")

    with open('data/input/sanitized_vif.txt', 'w') as txt_file:
        txt_file.write('0.0 0 0.0 0\n')
        for row in Vdata:
            line = ' '.join(str(v) for v in row)
            # works with any number of elements in a line
            txt_file.write(line + "\n")

    ax.plot(cT, cIfilt, 'b', linewidth=1)
    ax.set_xlabel('Time (s)', fontsize=18)
    ax.set_ylabel('Contrast Enhancement Intensity', fontsize=18)
    plt.show()

#Assuming that the original signal HAS BEEN sampled in accordance with the Nyquist condition (otherwise signal would alias),
#then, v>2*(f_max_of_the_signal).  To find the maximum freqency that should be allowed to pass through a low-pass filter 
#(used to get rid of noise), let us find f_max_of_the_signal and filter out any freqency above that.  To do this, find in the adaptively
#sampled signal the fastest/minimum (most restrictive on the Nyquist condition) sampling rate (v) and then solve for f_max_of_the_signal.
def findMaxAllowableFrequency(CTime):
    #Find the fastest sampling rate in the time course
    minSamplingRate = float('inf')
    for i in range(0,len(CTime)-1):
        diff = CTime[i+1] - CTime[i]
        print("Diff: ",diff)
        minSamplingRate = min(diff, minSamplingRate)
    print("Min. Sampling Rate: ",minSamplingRate)
    #Calculate and return the maximum allowed freq. in the sample
    print("Max. Allowable Freqency: ",minSamplingRate/2)
    return minSamplingRate/2
    
if __name__ == "__main__":
    main(sys.argv[1:])
