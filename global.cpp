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
