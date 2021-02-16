// SPDX-License-Identifier: GNU General Public License v3.0 or later

#include <Audio.h>
#include <arm_math.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 1

#define VERSION_UINT16 ((uint16_t)((VERSION_MAJOR<<8)|VERSION_MINOR))

// this converts to string
#define STR_(X) #X
// this makes sure the argument is expanded before converting to string
#define STR(X) STR_(X)

#define VERSION_STRING STR(VERSION_MAJOR) "." STR(VERSION_MINOR)

//#define DEBUG 1 //Enable to get serial port output

#define BUFFER_SIZE 128

// Decimate down to 11kHz - see if that helps the NR systems perform, as they will then not be trying
// to operate on the 5-20kHz data, which we never listen to anyway!
#define DF 4

#define FFT_L 512
#define FFT_length FFT_L

#define N_B (FFT_L / 2 / BUFFER_SIZE * DF)
#define N_BLOCKS N_B
extern uint32_t BUF_N_DF;
#define N_DEC_B (N_B / DF)

extern float32_t DMAMEM float_buffer_L[];
extern float32_t DMAMEM float_buffer_R[];
 
#define NR_MODE_COMPLETE_BYPASS 0
#define NR_MODE_OFF 1
#define NR_MODE_LMS 2
#define NR_MODE_KIM 3
#define NR_MODE_FNR 4
#define NR_MODE_FNRA 5
#define NR_MODE_SPECTRAL 6
#define NR_MODE_LLMS 7
#define NR_MODE_MAX 7
extern int nr_mode;   //current noise reduction mode

// NR stuff
#define NR_FFT_L    256
extern const arm_cfft_instance_f32 *NR_FFT;
extern const arm_cfft_instance_f32 *NR_iFFT;
extern float32_t DMAMEM NR_last_sample_buffer_L[];
extern float32_t DMAMEM NR_M[]; // minimum of the 20 last values of E
#define NR_N_frames 15
extern float32_t DMAMEM NR_E[NR_FFT_L / 2][NR_N_frames]; // averaged (over the last four values) X values for the last 20 FFT frames
extern float32_t DMAMEM NR_X[NR_FFT_L / 2][3]; // magnitudes (fabs) of the last four values of FFT results for 128 frequency bins
extern float32_t DMAMEM NR_G[NR_FFT_L / 2]; // preliminary gain factors (before time smoothing) and after that contains the frequency smoothed gain factors
extern float32_t DMAMEM NR_FFT_buffer [512] __attribute__ ((aligned (4)));
extern float32_t NR_alpha;
extern float32_t DMAMEM NR_last_iFFT_result [NR_FFT_L / 2];
extern float32_t DMAMEM NR_Gts[NR_FFT_L / 2][2]; // time smoothed gain factors (current and last) for each of the 128 bins

#define SAMPLE_RATE_MIN               6
#define SAMPLE_RATE_8K                0
#define SAMPLE_RATE_11K               1
#define SAMPLE_RATE_16K               2
#define SAMPLE_RATE_22K               3
#define SAMPLE_RATE_32K               4
#define SAMPLE_RATE_44K               5
#define SAMPLE_RATE_48K               6
#define SAMPLE_RATE_50K               7
#define SAMPLE_RATE_88K               8
#define SAMPLE_RATE_96K               9
#define SAMPLE_RATE_100K              10
#define SAMPLE_RATE_101K              11
#define SAMPLE_RATE_176K              12
#define SAMPLE_RATE_192K              13
#define SAMPLE_RATE_234K              14
#define SAMPLE_RATE_256K              15
#define SAMPLE_RATE_281K              16 // ??
#define SAMPLE_RATE_353K              17 // does not work !
#define SAMPLE_RATE_MAX               15

extern uint8_t SAMPLE_RATE;
extern uint8_t LAST_SAMPLE_RATE;

typedef struct SR_Descriptor
{
  const uint8_t SR_n;
  const uint32_t rate;
  const char* const text;
  const char* const f1;
  const char* const f2;
  const char* const f3;
  const char* const f4;
  const float32_t x_factor;
  const uint8_t x_offset;
} SR_Desc;

extern const SR_Descriptor SR [18];

// global decoder stuff
#define DECODER_OFF 0
#define DECODER_MORSE 1
#define DECODER_MORSE_K4ICY 2
#define DECODER_MORSE_TF3LJ 3
extern int decoder_mode;

//For morse decoder
extern int morse_cycles;
extern int morse_frequency;
extern float32_t morse_threshold;
extern AudioAnalyzeToneDetect toneDetect;

// FIR filter stuff
#define NUM_COEFFICIENTS  200
extern AudioFilterFIR firfilter;
extern short   fir_active1[];
extern int current_filter_mode;
extern void updateFilter();

extern bool nb_enabled;
extern int8_t NB_taps;

// noise blanker by Michael Wild
extern float32_t NB_thresh;
extern int8_t NB_taps;
extern int8_t NB_impulse_samples;

extern bool xanr_notch;

//AGC stuff
#define AGC_MODE_OFF 0    //Just pass on through
#define AGC_MODE_TRACK 1  //Try to track output peak/mean to input in sw
#define AGC_MODE_SG5K 2   //Enable the SGTL5000 AGC unit

extern int agc_mode;

extern AudioControlSGTL5000     sgtl5000_1;
extern int agc_sg5k_maxGain;
extern int agc_sg5k_response;
extern int agc_sg5k_hardLimit;
extern float32_t agc_sg5k_threshold;
extern float32_t agc_sg5k_attack;
extern float32_t agc_sg5k_decay;

// From the main loop
extern float32_t input_peak_acc;

//Display our output (or, is the menu active...)
extern bool display;

//Volume is actually a little complex - we have two inputs
// (usb and line-in), and two outputs (headphone and line-out).
// The usb input can supply a volume level value,
// and the sg5k volume only affects the headphone output.
// So, we don't actually have a line-in -> line-out volume
// control, per se. And then it gets sort of more complex
// when you mix in the agc stuff as well!
// Generally I'd say operate with the 5k AGC enabled, and adjust
// your rx set to provide 'decent' line-in levels (you can see them
// on the input level meter), and adjust whatever you have hanging
// on the line-out (amplified speakers?) for your output levels.
// If you are using headphones, then adjust via the menu to tweak
// this setting.
// If you are connected to USB, then use the host computer to change
// the headphone setting - and line, in level.
// This setting is *only* used to adjust headphone volume when we are
// not connected to USB (or, the USB is set to 0 volume). afaict, there
// is no way to tell when we are *not* connected to USB?
// Maybe that is a feature to add to the prjc audio library..
extern float32_t global_volume;    //A good default volume


//TF3LJ morse decoder
extern AudioAnalyzeFFT256 morse_fft;
