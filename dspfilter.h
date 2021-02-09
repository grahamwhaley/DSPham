#ifndef DSPFILTER_H
#define DSPFILTER_H


// User defined filters ID's
#define FILTER_PASSTHRU  0
#define FILTER_SSB       1
#define FILTER_CW        2
#define FILTER_AM        3
#define FILTER_FM        4
#define FILTER_USER      5

// Single filter structure
struct filter {
  const short int     filterID;
  const short int     filterType;
  double              freqLow;
  double              freqHigh;
  const short int     window;                       // Windows included are Blackman, Hanning, and Hamming
  const short int     coeff;
};

extern struct filter filterList[];

#define NUM_FIR_FILTERS (sizeof[filterList]/sizeof[filterList[0])

extern unsigned int filterIndex;                 // index to currently selected filter above
extern short   fir_active1[];                    // 1st DSP filter array holding the coefficient as 32bit (short)

#endif
