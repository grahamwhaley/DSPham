//Taken from Teensy convolution SDR - which shares it with UHSDR?
//Originally from Warren Pratts wdsp library

#include <Audio.h>
#include <arm_math.h>
#include <arm_const_structs.h>

#include "global.h"


// Automatic noise reduction
// Variable-leak LMS algorithm
// taken from (c) Warren Pratts wdsp library 2016
// GPLv3 licensed
#define ANR_DLINE_SIZE 512 //funktioniert nicht, 128 & 256 OK                 // dline_size
int ANR_taps =     64; //64;                       // taps
int ANR_delay =    32; //16;                       // delay // Graham - 32 seems to reduce noise more.
int ANR_dline_size = ANR_DLINE_SIZE;
int ANR_buff_size = FFT_length / 2.0;
float32_t ANR_two_mu =   0.0001;                     // two_mu --> "gain"
float32_t ANR_gamma =    0.1;                      // gamma --> "leakage" //Graham, increasing helps noise, but kills noise blanker
float32_t ANR_lidx =     120.0;                      // lidx
//float32_t ANR_lidx_min = 0.0;                      // lidx_min
float32_t ANR_lidx_min = 120.0;                      // lidx_min
float32_t ANR_lidx_max = 200.0;                      // lidx_max
float32_t ANR_ngamma =   0.001;                      // ngamma
float32_t ANR_den_mult = 6.25e-10;                   // den_mult
float32_t ANR_lincr =    1.0;                      // lincr
float32_t ANR_ldecr =    3.0;                     // ldecr
int ANR_mask = ANR_dline_size - 1;
int ANR_in_idx_nr = 0;
int ANR_in_idx_notch = 0;
//FIXME - having these as DMAMEM causes a hang/fail on startup in the
// init routine it seems - why ??
//float32_t DMAMEM ANR_d [ANR_DLINE_SIZE];
//float32_t DMAMEM ANR_w [ANR_DLINE_SIZE];
float32_t ANR_d_nr [ANR_DLINE_SIZE];
float32_t ANR_w_nr [ANR_DLINE_SIZE];
float32_t ANR_d_notch [ANR_DLINE_SIZE];
float32_t ANR_w_notch [ANR_DLINE_SIZE];
uint8_t ANR_on = 0;
uint8_t ANR_notch = 0;

void xanr_init() {
  for(unsigned i = 0; i < ANR_DLINE_SIZE; i++)
  {
      ANR_d_nr[i] = 0.0;
      ANR_w_nr[i] = 0.0;
      ANR_d_notch[i] = 0.0;
      ANR_w_notch[i] = 0.0;
  }
}

void xanr (bool notch) // variable leak LMS algorithm for automatic notch or noise reduction
{ // (c) Warren Pratt wdsp library 2016
  int idx;
  float32_t c0, c1;
  float32_t y, error, sigma, inv_sigp;
  float32_t nel, nev;
  float32_t *ANR_d, *ANR_w;
  int ANR_in_idx;

  //Separate history buffers for notch and nr, so we can run both at once.
  if (notch) {
    ANR_d = ANR_d_notch;
    ANR_w = ANR_w_notch;
    ANR_in_idx = ANR_in_idx_notch;
  } else {
    ANR_d = ANR_d_nr;
    ANR_w = ANR_w_nr;    
    ANR_in_idx = ANR_in_idx_nr;
  }
  for (int i = 0; i < ANR_buff_size; i++)
  {
    //      ANR_d[ANR_in_idx] = in_buff[2 * i + 0];
    ANR_d[ANR_in_idx] = float_buffer_L[i];

    y = 0;
    sigma = 0;

    for (int j = 0; j < ANR_taps; j++)
    {
      idx = (ANR_in_idx + j + ANR_delay) & ANR_mask;
      y += ANR_w[j] * ANR_d[idx];
      sigma += ANR_d[idx] * ANR_d[idx];
    }
    inv_sigp = 1.0 / (sigma + 1e-10);
    error = ANR_d[ANR_in_idx] - y;

    if (notch) float_buffer_R[i] = error; // NOTCH FILTER
    else  float_buffer_R[i] = y; // NOISE REDUCTION

    if ((nel = error * (1.0 - ANR_two_mu * sigma * inv_sigp)) < 0.0) nel = -nel;
    if ((nev = ANR_d[ANR_in_idx] - (1.0 - ANR_two_mu * ANR_ngamma) * y - ANR_two_mu * error * sigma * inv_sigp) < 0.0) nev = -nev;
    if (nev < nel)
    {
      if ((ANR_lidx += ANR_lincr) > ANR_lidx_max) ANR_lidx = ANR_lidx_max;
      else if ((ANR_lidx -= ANR_ldecr) < ANR_lidx_min) ANR_lidx = ANR_lidx_min;
    }
    ANR_ngamma = ANR_gamma * (ANR_lidx * ANR_lidx) * (ANR_lidx * ANR_lidx) * ANR_den_mult;

    c0 = 1.0 - ANR_two_mu * ANR_ngamma;
    c1 = ANR_two_mu * error * inv_sigp;

    for (int j = 0; j < ANR_taps; j++)
    {
      idx = (ANR_in_idx + j + ANR_delay) & ANR_mask;
      ANR_w[j] = c0 * ANR_w[j] + c1 * ANR_d[idx];
    }
    ANR_in_idx = (ANR_in_idx + ANR_mask) & ANR_mask;
  }
  if (notch) {
    ANR_in_idx_notch = ANR_in_idx;
  } else {
    ANR_in_idx_nr = ANR_in_idx;
  }

}
