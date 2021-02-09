// SPDX-License-Identifier: GNU General Public License v3.0 or later

#include <Audio.h>
#include <arm_math.h>
#include <arm_const_structs.h>

#include "global.h"

uint32_t BUF_N_DF = BUFFER_SIZE * N_BLOCKS / (uint32_t)DF;
float32_t DMAMEM float_buffer_L [BUFFER_SIZE * N_B];
float32_t DMAMEM float_buffer_R [BUFFER_SIZE * N_B];

// NR stuff - shared by Kim and spectral at least.
const static arm_cfft_instance_f32 *NR_FFT;
const static arm_cfft_instance_f32 *NR_iFFT;
float32_t DMAMEM NR_last_sample_buffer_L [NR_FFT_L / 2];
float32_t DMAMEM NR_M[NR_FFT_L / 2]; // minimum of the 20 last values of E
//now define const uint8_t NR_N_frames = 15; // default 24 //40 //12 //20 //18//12 //20
float32_t DMAMEM NR_E[NR_FFT_L / 2][NR_N_frames]; // averaged (over the last four values) X values for the last 20 FFT frames
float32_t DMAMEM NR_X[NR_FFT_L / 2][3]; // magnitudes (fabs) of the last four values of FFT results for 128 frequency bins
float32_t DMAMEM NR_G[NR_FFT_L / 2]; // preliminary gain factors (before time smoothing) and after that contains the frequency smoothed gain factors
float32_t DMAMEM NR_FFT_buffer [512] __attribute__ ((aligned (4)));
float32_t NR_alpha = 0.95; // default 0.99 --> range 0.98 - 0.9999; 0.95 acts much too hard: reverb effects
float32_t DMAMEM NR_last_iFFT_result [NR_FFT_L / 2];
float32_t DMAMEM NR_Gts[NR_FFT_L / 2][2]; // time smoothed gain factors (current and last) for each of the 128 bins

uint8_t SAMPLE_RATE =            SAMPLE_RATE_44K;
uint8_t LAST_SAMPLE_RATE =       SAMPLE_RATE_44K;

const SR_Descriptor SR [18] =
{ // x_factor, x_offset and f1 to f4 are NOT USED ANYMORE !!!
  //   SR_n , rate, text, f1, f2, f3, f4, x_factor = pixels per f1 kHz in spectrum display
  {  SAMPLE_RATE_8K, 8000,  "  8k", " 1", " 2", " 3", " 4", 64.0, 11}, // not OK
  {  SAMPLE_RATE_11K, 11025, " 11k", " 1", " 2", " 3", " 4", 43.1, 17}, // not OK
  {  SAMPLE_RATE_16K, 16000, " 16k",  " 4", " 4", " 8", "12", 64.0, 1}, // OK
  {  SAMPLE_RATE_22K, 22050, " 22k",  " 5", " 5", "10", "15", 58.05, 6}, // OK
  {  SAMPLE_RATE_32K, 32000,  " 32k", " 5", " 5", "10", "15", 40.0, 24}, // OK, one more indicator?
  {  SAMPLE_RATE_44K, 44100,  " 44k", "10", "10", "20", "30", 58.05, 6}, // OK
  {  SAMPLE_RATE_48K, 48000,  " 48k", "10", "10", "20", "30", 53.33, 11}, // OK
  {  SAMPLE_RATE_50K, 50223,  " 50k", "10", "10", "20", "30", 53.33, 11}, // NOT OK
  {  SAMPLE_RATE_88K, 88200,  " 88k", "20", "20", "40", "60", 58.05, 6}, // OK
  {  SAMPLE_RATE_96K, 96000,  " 96k", "20", "20", "40", "60", 53.33, 12}, // OK
  {  SAMPLE_RATE_100K, 100000,  "100k", "20", "20", "40", "60", 53.33, 12}, // NOT OK
  {  SAMPLE_RATE_101K, 100466,  "101k", "20", "20", "40", "60", 53.33, 12}, // NOT OK
  {  SAMPLE_RATE_176K, 176400,  "176k", "40", "40", "80", "120", 58.05, 6}, // OK
  {  SAMPLE_RATE_192K, 192000,  "192k", "40", "40", "80", "120", 53.33, 12}, // not OK
  {  SAMPLE_RATE_234K, 234375,  "234k", "40", "40", "80", "120", 53.33, 12}, // NOT OK
  {  SAMPLE_RATE_256K, 256000,  "256k", "40", "40", "80", "120", 53.33, 12}, // NOT OK
  {  SAMPLE_RATE_281K, 281000,  "281k", "40", "40", "80", "120", 53.33, 12}, // NOT OK
  {  SAMPLE_RATE_353K, 352800,  "353k", "40", "40", "80", "120", 53.33, 12} // NOT OK
};

int nr_mode = NR_MODE_SPECTRAL;

// Is the menu active, or should we 'display' our status and decoder output etc.
bool display = true;

// Decoder stuff
int decoder_mode = DECODER_OFF;

//For morse decoder
// Fastest we might decode, let's say, 50wmp.
// one 'wpm' is '50 dit' spaces (the 'Paris' notion)
// 60(seconds) / 50 (dits) * 50 (wpm) == 24mS
// 600 (Hz) * 0.024 (S) == 14.4 cycles for a very fast 'dit'
int morse_cycles = 14;
int morse_frequency = 600;
float32_t morse_threshold = 0.01;   //Pretty low by default.

// noise blanker by Michael Wild
bool nb_enabled = true;
float32_t NB_thresh = 2.5;
int8_t NB_taps = 10;
int8_t NB_impulse_samples = 7;

bool xanr_notch = true; //Default true, as it is very useful, and has no bad side effects.

int agc_mode = AGC_MODE_SG5K;

//Values inspired from the pjrc audio design tool. Also check the 5k docs!
int agc_sg5k_maxGain = 2; //1; //6db max
int agc_sg5k_response = 1; //2;  //50ms
int agc_sg5k_hardLimit = 0;   //soft knee
float32_t agc_sg5k_threshold = -5; //-18; //dB's
float32_t agc_sg5k_attack = 0.5; //5;  //in dB/s
float32_t agc_sg5k_decay = 0.5; //1;   //in dB/s - generally lower than the attack

float32_t global_volume = 0.5;    //A good default volume
