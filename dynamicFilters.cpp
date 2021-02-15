// From the $19 dsp filter project
// https://github.com/gi1mic/19Dollar-DSP-Filter
// MIT licensed.

#include <Audio.h>
#include <math.h>

#include "dynamicFilters.h"
#include "dspfilter.h"
#include "global.h"

unsigned int filterIndex = 0;                 // index to currently selected filter above
short   fir_active1[200];                      // 1st DSP filter array holding the coefficient as 32bit (short)

/*
 *   Structure to hold the required filters (Add, delete or modify as required) 
 *   
 *   ID,      FilterType,   Low Freq,      Hi Freq,    Window,  FilterName  
 */  
struct filter filterList[] = {
  {FILTER_PASSTHRU, ID_BANDPASS,   60.0,  20000.0, W_HAMMING,  NUM_COEFFICIENTS},
  {FILTER_SSB,      ID_BANDPASS,  300.0,   2700.0, W_HAMMING,  NUM_COEFFICIENTS},
  {FILTER_CW,       ID_BANDPASS,  450.0,    950.0, W_HAMMING,  NUM_COEFFICIENTS},
  {FILTER_AM,       ID_BANDPASS,   50.0,  11000.0, W_HAMMING,  NUM_COEFFICIENTS},   //Xiegu G90 has 10.8Khz AM bandwidth
  {FILTER_FM,       ID_BANDPASS,   50.0,  16000.0, W_HAMMING,  NUM_COEFFICIENTS},   //Guess at a wide FM bandwidth
};


double  fir_tmp[NUM_COEFFICIENTS];                          // Temp array used for coefficient calculations which are performed in 64bit


//---------------------------------------------------------------
// Generate lowpass filter
//
// This is done by generating a sinc function and then windowing it
void wsfirLP(double h[],    // h[] will be written with the filter coefficients
             const int &N,    // size of the filter (number of taps)
             const int &WINDOW, // window function (W_BLACKMAN, W_HANNING, etc.)
             const double &fc)  // cutoff frequency
{
  int i;
  double *w = new double[N];    // window function
  double *sinc = new double[N]; // sinc function

  // 1. Generate Sinc function
  genSinc(sinc, N, fc);

  // 2. Generate Window function
  switch (WINDOW) {
    case W_BLACKMAN:  // W_BLACKMAN
      wBlackman(w, N);
      break;
    case W_HANNING:   // W_HANNING
      wHanning(w, N);
      break;
    case W_HAMMING:   // W_HAMMING
      wHamming(w, N);
      break;
    default:
      break;
  }

  // 3. Make lowpass filter
  for (i = 0; i < N; i++) {
    h[i] = sinc[i] * w[i];
  }

  // Delete dynamic storage
  delete []w;
  delete []sinc;

  return;
}


//---------------------------------------------------------------
// Generate highpass filter
//
// This is done by generating a lowpass filter and then spectrally inverting it
void wsfirHP(double h[],    // h[] will be written with the filter coefficients
             const int &N,    // size of the filter
             const int &WINDOW, // window function (W_BLACKMAN, W_HANNING, etc.)
             const double &fc)  // cutoff frequency
{
  int i;

  // 1. Generate lowpass filter
  wsfirLP(h, N, WINDOW, fc);

  // 2. Spectrally invert (negate all samples and add 1 to center sample) lowpass filter
  // = delta[n-((N-1)/2)] - h[n], in the time domain
  for (i = 0; i < N; i++) {
    h[i] *= -1.0; // = 0 - h[i]
  }
  h[(N - 1) / 2] += 1.0; // = 1 - h[(N-1)/2]

  return;
}

//---------------------------------------------------------------
// Generate bandstop filter
//
// This is done by generating a lowpass and highpass filter and adding them
void wsfirBS(double h[],    // h[] will be written with the filter taps
             const int &N,    // size of the filter
             const int &WINDOW, // window function (W_BLACKMAN, W_HANNING, etc.)
             const double &fc1, // low cutoff frequency
             const double &fc2) // high cutoff frequency
{
  int i;
  double *h1 = new double[N];
  double *h2 = new double[N];

  // 1. Generate lowpass filter at first (low) cutoff frequency
  wsfirLP(h1, N, WINDOW, fc1);

  // 2. Generate highpass filter at second (high) cutoff frequency
  wsfirHP(h2, N, WINDOW, fc2);

  // 3. Add the 2 filters together
  for (i = 0; i < N; i++) {
    h[i] = h1[i] + h2[i];
  }

  // Delete dynamic memory
  delete []h1;
  delete []h2;

  return;
}

//---------------------------------------------------------------
// Generate bandpass filter
//
// This is done by generating a bandstop filter and spectrally inverting it
void wsfirBP(double h[],        // h[] will be written with the filter taps
             const int &N,            // size of the filter
             const int &WINDOW,       // window function (W_BLACKMAN, W_HANNING, etc.)
             const double &fc1,       // low cutoff frequency
             const double &fc2)       // high cutoff frequency
{
  int i;

  // 1. Generate bandstop filter
  wsfirBS(h, N, WINDOW, fc1, fc2);

  // 2. Spectrally invert bandstop (negate all, and add 1 to center sample)
  for (i = 0; i < N; i++) {
    h[i] *= -1.0; // = 0 - h[i]
  }
  h[(N - 1) / 2] += 1.0; // = 1 - h[(N-1)/2]

  return;
}

//---------------------------------------------------------------
// Generate sinc function - used for making lowpass filter
void genSinc(double sinc[],   // sinc[] will be written with the sinc function
             const int &N,          // size (number of taps)
             const double &fc)      // cutoff frequency
{
  int i;
  const double M = N - 1;
  double n;

  // Generate sinc delayed by (N-1)/2
  for (i = 0; i < N; i++) {
    if (i == M / 2.0) {
      sinc[i] = 2.0 * fc;
    }
    else {
      n = (double)i - M / 2.0;
      sinc[i] = sin(2.0 * M_PI * fc * n) / (M_PI * n);
    }
  }

  return;
}

//---------------------------------------------------------------
// Generate window function (Blackman)
void wBlackman(double w[],    // w[] will be written with the Blackman window
               const int &N)        // size of the window
{
  int i;
  const double M = N - 1;

  for (i = 0; i < N; i++) {
    w[i] = 0.42 - (0.5 * cos(2.0 * M_PI * (double)i / M)) + (0.08 * cos(4.0 * M_PI * (double)i / M));
  }

  return;
}


//---------------------------------------------------------------
// Generate window function (Hanning)
void wHanning(double w[],   // w[] will be written with the Hanning window
              const int &N)       // size of the window
{
  int i;
  const double M = N - 1;

  for (i = 0; i < N; i++) {
    w[i] = 0.5 * (1.0 - cos(2.0 * M_PI * (double)i / M));
  }

  return;
}


//---------------------------------------------------------------
// Generate window function (Hamming)
void wHamming(double w[],   // w[] will be written with the Hamming window
              const int &N)       // size of the window
{
  int i;
  const double M = N - 1;

  for (i = 0; i < N; i++) {
    w[i] = 0.54 - (0.46 * cos(2.0 * M_PI * (double)i / M));
  }

  return;
}

//---------------------------------------------------------------
void coeffConvert(double in[], short out[], const int &N) {
#ifdef SHOWCOEFF
  Serial.println(";---- Cut and paste into a text file (.coe extension) -----");
  Serial.println(";------------ for import and analysis by MATLAB -----------");
  Serial.println("; FIR Filter");
  Serial.println("Radix = 10;");
  Serial.print("CoefData= ");
  for (int j = 0; j < N; j++) {
    //out[j] = (short)(in[j] * 10000);
    out[j] = (short)(in[j] * 32767);
    Serial.print(out[j]);
    Serial.print(", ");
  }

  Serial.println("0;");
  Serial.println("; ---------------------- End Cut ---------------------------");
#else
  for (int j = 0; j < N; j++) {
    //out[j] = (short)(in[j] * 10000);
    out[j] = (short)(in[j] * 32767);
  };
#endif
}

//---------------------------------------------------------------
void lowpass(short h[], const int &N, const int &WINDOW, const double &fc) {
        wsfirLP(fir_tmp, N, WINDOW, fc );
        coeffConvert(fir_tmp, h, N);
}

//---------------------------------------------------------------
void highpass(short h[], const int &N, const int &WINDOW, const double &fc) {
        wsfirHP(fir_tmp, N, WINDOW, fc);
        coeffConvert(fir_tmp, h, N);
}

//---------------------------------------------------------------
void bandpass(short h[], const int &N, const int &WINDOW, const double &fc1, const double &fc2) {
        wsfirBP(fir_tmp, N, WINDOW, fc1, fc2 );
        coeffConvert(fir_tmp, h, N);
}

//---------------------------------------------------------------
void bandstop(short h[], const int &N, const int &WINDOW, const double &fc1, const double &fc2) {
        wsfirBS(fir_tmp, N, WINDOW, fc1, fc2 );
        coeffConvert(fir_tmp, h, N);
}

//---------------------------------------------------------------
void audioFilter(short h[], const int &N, const int &TYPE, const int &WINDOW, const double &fc1, const double &fc2) {
  switch (TYPE) {
      case ID_LOWPASS:
                  #ifdef DEBUG
                      Serial.print("LowPass Freq:");
                      Serial.println(fc1);
                  #endif
                  lowpass(h, N, WINDOW, fc1/44117);
                  break;
      case ID_HIGHPASS:
                  #ifdef DEBUG
                      Serial.print("HiPass Freq:");
                      Serial.println(fc1);
                  #endif
                  highpass(h, N, WINDOW, fc1/44117);
                  break;
      case ID_BANDPASS:
                  #ifdef DEBUG
                      Serial.print("BandPass LFreq:");
                      Serial.print(fc1);
                      Serial.print(" HFreq:");
                      Serial.println(fc2);
                  #endif
                  bandpass(h, N, WINDOW, fc1/44117, fc2/44117);
                  break;
      case ID_BANDSTOP:
                  #ifdef DEBUG
                      Serial.print("Bandstop LFreq:");
                      Serial.print(fc1);
                      Serial.print(" HFreq:");
                      Serial.println(fc2);
                  #endif
                  bandstop(h, N, WINDOW, fc1/44117, fc2/44117);
                  break;
      default:
                  #ifdef DEBUG
                      Serial.println("Unknown");    
                  #endif
                  break;
  }                  
}

//Additions from Graham
// The filters come out with some attenuation - too much for my linking.
// If we can measure the gain at the requested bandpass, then we can scale
// the coefficients to actual 0db (neutral, no gain or attenuation) for the
// chosen frequency, which should, ideally, be the peak of the filter.
//
// Gain is the root of the sum of the squares of cos and sin waves. Don't ask me for
// the theory!!!
float32_t getFilterGain(int16_t *coeffs, int ncoeffs, float32_t frequency, float32_t samplerate) {
  float32_t cgain=0.0, sgain=0.0;
  float32_t gain = 0.0;
  float32_t f = frequency / samplerate;

  for(int i=0; i<ncoeffs; i++ ) {
    float32_t c = cos(2.0 * M_PI * f * i);
    float32_t s = sin(2.0 * M_PI * f * i);

    cgain += ((float32_t)coeffs[i]/32768.0) * c;
    sgain += ((float32_t)coeffs[i]/32768.0) * s;
  }

  gain = (cgain*cgain) + (sgain*sgain);

  //FIXME - we probably should limit the gain returned somewhere - possibly here.
  return (sqrt(gain));
}

void normaliseCoeffs(int16_t *coeffs, int ncoeffs, float32_t multiplier) {
  for( int i=0; i<ncoeffs; i++ ) {
    coeffs[i] *= multiplier;
  }
}
