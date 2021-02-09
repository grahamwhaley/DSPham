// Taken from https://sites.google.com/site/lofturj/cwreceive
// License: GPLv3
//

#include <Audio.h>
#include <arm_math.h>

#include "global.h"
#include "morseGen.h"

#include "tf3lj.h"


int16_t peakFrq;
float32_t agcvol = 1.0;
int16_t vol = 1024;      //Default full volume - we rely on the input volume and the AGC. This vol is redundant for us.
int16_t     fftbuf[60];                    // FFT output buffer. Updated by SignalSampler() Interrupt function.
int16_t     thresh       = 1;     // Audio threshold level (0 - 40)
bool        state;                // Current decoded signal state
sigbuf      sig[SIG_BUFSIZE];     // A circular buffer of decoded input levels and durations, input from
int32_t     sig_lastrx   = 0;     // Circular buffer in pointer, updated by SignalSampler
int32_t     sig_incount  = 0;     // Circular buffer in pointer, copy of sig_lastrx, used by CW Decode functions
int32_t     sig_outcount = 0;     // Circular buffer out pointer, used by CW Decode functions
 // Elapsed time of current signal state, in units of the
 // FFT conversion time. approx 2.9ms for FFT256 with no averaging
 // or 11.6ms for FFT1024 with no averaging.  Updated by SignalSampler.
int32_t     sig_timer    = 0;
int32_t timer_stepsize=FFTAVERAGE;    // Step size of signal timer depends on FFT conversion time, FFT1024=4
uint8_t data_len;
int32_t               cur_time;                     // copy of sig_timer

void tf3lj_init(void) {
  morse_fft.windowFunction(AudioWindowBlackmanNuttall1024); // Good overall rejection characteristics

  //FIXME - this needs to be done every time we change the freq etc.
  //peakFrq = (44100/1024) * morse_frequency;              // BW_of_bin * number of bin
  peakFrq = morse_frequency;
}

//------------------------------------------------------------------
//
// Signal Sampler and Change Detection Function
// Interrupt driven, polls the FFT function once every 200us
// to see whether it has new data.  Either 256 or 1024 FFT is
// used to identify tone,
//
// Output is a circular buffer, sig[SIG_BUFSIZE], containing
// timing information for High & Low states.
//
//------------------------------------------------------------------
void tf3lj_process(void)
{
  static int16_t  siglevel;                 // FFT signal level
  int16_t         lvl=0;                    // Multiuse variable
  int16_t         pklvl;                    // Used for AGC calculations
  int16_t         pk;                       // FFT bin containing peak level
  static bool     prevstate;                // Last recorded state of signal input (mark or space)
  static bool     toneout;                  // Keep track of state changes for tone out

  //----------------
  // Automatic Gain Control (AGC) - using level at peakFrq as basis 
#if 1 //fft256
  pk = 4*peakFrq/172 - 1;                       // FFT256 frequency bin number of peak frequency
  pklvl = agcvol * vol * morse_fft.read(peakFrq/172); // Get level at peak frequency
#else //fft1024
  pk = peakFrq/43 - 5;                          // FFT1024 frequency bin number of peak frequency
  pklvl = agcvol * vol * morse_fft.read(peakFrq/43); // Get level at peak frequency
#endif

  if (pklvl > 45) agcvol = agcvol * AGC_ATTACK;   // Decrease volume if above this level.
  if (pklvl < 40) agcvol = agcvol * AGC_DECAY;    // Increase volume if below this level.

  //if (agcvol > 1.0) agcvol = 1.0;                 // Cap max at 1.0
  //We see 'good' morse values around 0.002 in the bucket for low level morse.
  //Our volume is set to 1024, and we want to get to about '42.5' (between 40 and 45) with the AGC.
  //Thus, our max agc is going to be about 42.5/(1024*.01) == ~ 4 ??
#define AGC_MAX 1.5
  if (agcvol > AGC_MAX) agcvol = AGC_MAX;

#if 1 //fft256
  for (uint8_t x=0; x<60; x++)
  {
    // Get signal level at each frequency, start at 215 Hz (5 * 43 Hz)
    if (x%4==0) lvl= agcvol * vol *morse_fft.read((x+5)/4);  // Only read every fourth time
    if (lvl > 40) lvl=40;                       // Cap max at 40
    fftbuf[x] = lvl;
    siglevel = fftbuf[pk];                      // Fetch the signal level at peak frequency
  }

#else //fft1024
  // Scan through all relevant FFT bins (215 - 2800 Hz)
  // collect the relevant ones around peakFrq - and plot all to LCD
  for (uint8_t x=0; x<60; x++)
  {
    // Get signal level at each frequency, start at 215 Hz (5 * 43 Hz)
    lvl = agcvol * vol * morse_fft.read(x+5);
    if (lvl > 40) lvl = 40;                     // Cap max at 40
    fftbuf[x] = lvl;     
    // Average the signal level in 1, 2 ... 5 FFT bins around the Peak frequency
    #if   FILTERBW == 5
    siglevel = (fftbuf[pk-2]+fftbuf[pk-1]+fftbuf[pk]+fftbuf[pk+1]+fftbuf[pk+2])/3;
    #elif FILTERBW == 4
    siglevel = (fftbuf[pk-1]+fftbuf[pk]+fftbuf[pk+1]+fftbuf[pk+2])/2;
    #elif FILTERBW == 3
    siglevel = (fftbuf[pk-1]+fftbuf[pk]+fftbuf[pk+1])/2;
    #elif FILTERBW == 2
    (FILTERBW==2) siglevel = (fftbuf[pk]+fftbuf[pk+1])/2;
    #else
    siglevel = fftbuf[pk];
    #endif               
  }
#endif

  //----------------
  // Signal averaging (smoothing)
  static int16_t avg_win[SIGAVERAGE];             // Sliding window buffer for signal averaging, if used                           
  static uint8_t avg_cnt;                         // Sliding window counter
  avg_win[avg_cnt++] = siglevel;                  // Add value onto "sliding window" buffer  
  if (avg_cnt == SIGAVERAGE) avg_cnt = 0;         // and roll counter             
  lvl = 0;
  for (uint8_t x=0; x<SIGAVERAGE; x++)            // Average up all values within sliding window
  {
    lvl = lvl + avg_win[x];
  }
  siglevel = lvl/SIGAVERAGE;

  //----------------
  // Signal State sampling
  #if NOISECANCEL                                 // Noise Cancel function requires two consecutive
  static bool newstate, change;                   // reads to be the same to confirm a true change

if(0){
  char buf[64];
  sprintf(buf, "postavg: Sig thresh:%d Sig:%d\n", thresh, siglevel);
  Serial.print(buf);
}

  if (siglevel >= thresh) {
    newstate = TRUE;
  }  else {
    newstate = FALSE;
  }
  if (change == TRUE)
  {
    state = newstate;
    change = FALSE;
  }
  else if (newstate != state) change = TRUE;
  #else                                           // No noise canceling
  if (siglevel >= thresh) state = TRUE;
  else                    state = FALSE;
  #endif

  //----------------
  // Record state changes and durations onto circular buffer
  if (state != prevstate)
  {
    // Enter the type and duration of the state change into the circular buffer
    sig[sig_lastrx].state  = prevstate;
    sig[sig_lastrx++].time = sig_timer;
    // Zero circular buffer when at max
    if (sig_lastrx == SIG_BUFSIZE) sig_lastrx = 0;
    sig_timer = 0;                                // Zero the signal timer.
    prevstate = state;                            // Update state
  }

  //----------------
  // Count signal state timer upwards based on which sampling rate is in effect
  sig_timer = sig_timer + timer_stepsize;
  if (sig_timer>=344*TIMEOUT) sig_timer = 344*TIMEOUT; // Impose a MAXTIME second boundary for overflow time 

  //static bool debug;
  //digitalWrite(LED_PIN, debug ^= 1);            // Debug - measure rate of FFT
  digitalWrite(LED_PIN, state);                   // Show current state on LED

  //----------------
  // Tone to Speaker^H^H LED when mark (key-down)
  if (toneout != state) 
  {
    if (state) morseLed(true);
    else       morseLed(false);
    toneout = state;
  } 
if(0) Serial.println("");
}
