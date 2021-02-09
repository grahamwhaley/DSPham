

//*********************************************************************************
//**
//** Project.........: Read Hand Sent Morse Code (tolerant of considerable jitter)
//**
//** Copyright (c) 2016  Loftur E. Jonasson  (tf3lj [at] arrl [dot] net)
//**
//** This program is free software: you can redistribute it and/or modify
//** it under the terms of the GNU General Public License as published by
//** the Free Software Foundation, either version 3 of the License, or
//** (at your option) any later version.
//**
//** This program is distributed in the hope that it will be useful,
//** but WITHOUT ANY WARRANTY; without even the implied warranty of
//** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//** GNU General Public License for more details.
//**
//** The GNU General Public License is available at
//** http://www.gnu.org/licenses/
//**
//** Substantive portions of the methodology used here to decode Morse Code are found in:
//**
//** "MACHINE RECOGNITION OF HAND-SENT MORSE CODE USING THE PDP-12 COMPUTER"
//** by Joel Arthur Guenther, Air Force Institute of Technology,
//** Wright-Patterson Air Force Base, Ohio
//** December 1973
//** http://www.dtic.mil/dtic/tr/fulltext/u2/786492.pdf
//**
//** Platform........: Teensy 3.1 / 3.2 and the Teensy Audio Shield
//**                   & a 160x128 TFT LCD
//**
//** Initial version.: 0.00, 2016-01-25  Loftur Jonasson, TF3LJ / VE2LJX
//**
//*********************************************************************************


//Graham - FIXME - these are not appropriate now integrated into a different project.
#define  VERSION "0.42"
#define  DATE    "2016-03-11"

extern void tf3lj_init(void);
extern void tf3lj_process(void);

//
//-----------------------------------------------------------------------------
// Don't touch any of the stuff below - unless you are messing directly with the code
//
// Miscellaneous software defines, functions and variables
//-----------------------------------------------------------------------------
//
typedef struct {
                unsigned state       :  1; // Pulse or space (sample buffer) OR Dot or Dash (data buffer)
                unsigned time        : 31; // Time duration
               } sigbuf;

typedef struct {
                unsigned initialized :  1; // Do we have valid time duration measurements?
                unsigned dash        :  1; // Dash flag
                unsigned wspace      :  1; // Word Space flag
                unsigned timeout     :  1; // Timeout flag
                unsigned overload    :  1; // Overload flag
               } bflags;
//-----------------------------------------------------------------------------

extern int32_t cur_time;
extern uint8_t data_len;
extern int32_t sig_incount;
extern int32_t sig_lastrx;
extern int32_t sig_timer;
extern int32_t sig_outcount;
extern sigbuf      sig[];


//
// Note on performance:
// 1024 point and 256 point FFT are user selectable in real time with FFT_SEL_PIN. While the 
// 1024 FFT provides a much better frequency filter resolution than the 256 fft, the tradeoff
// is that it provides a lower sampling rate. Approx 11.6ms per sample(512/44100Hz) for a 
// 1024 point FFT vs. 2.9ms (128/44100Hz) for 256 point FFT. 
// When using the 1024 FFT, at 20 WPM, the sampling rate is 5 times the "dot" rate, probably 
// the highest one can go without starting to notice loss in performance.  So if decoding
// higher rates, then the 256 FFT should be used.
//
// While the user selectable filtering methods below aim at reducing error rate, they all equate
// to reducing the sampling rate somewhat, and hence impact the performance at higher rates.
//

//-----------------------------------------------------------------------------
// Features and Pins Selection
//-----------------------------------------------------------------------------  

//-----------------------------------------------------------------------------  
// Operational parameters                             

#define TIMEOUT            3  // Time, in seconds, to trigger display of last Character received
                              // and a New Line in the USB Serial Monitor.

#define RATE_TO_SERIAL     1  // 1 to print detailed CW Rate to Serial Port whenever TIMEOUT, else 0.

#define AUDIOOUT_LEVEL   0.3  // Level of sinewave output for the Audio out CW Decode Monitor.

#define AGC_ATTACK      0.95  // Audio automatic gain control (AGC) attack, audio vol reduce per cycle.
#define AGC_DECAY      1.005  // Audio AGC decay, audio volume increase per cycle.
                              // AGC attempts to cap the max signal level at the Fpeak frequency to 40
                              // (40 is arbitrarily picked, is max in FFT bargraph).

#define FILTERBW           3  // If 1024 FFT, then this determines bandwidth of filter in number of FFT bins.
                              // Each bin is 43 Hz wide. Valid values are 1 - 5. 3 seems to be a good number.

//-----------------------------------------------------------------------------  
// Selection of all sorts of post-filtering, including noise/spike/dropout cancel                           
#define SIGAVERAGE         2  // N = 1, 2, 3... Probably a better method than FFTAVERAGE.  Also works
                              // with FFT1024.  Averages (smoothes) signal from N number of samples,
                              //  but does not slow down sampling rate.  Fights drops and spikes.

#define FFTAVERAGE         2  // N = 1, 2, 3... Used with FFT256.  Averages consecutive FFT reads.
                              // 2 doubles the the time per conversion.  Does nothing when FFT1024.
                              // Timing is very jittery, when 1.
                              
#define NOISECANCEL        1  // Noise Cancellation by requiring two consecutive reads to be the same
                              // for a state change.  1 to select, 0 to deselect. Normally 1.

#define SPIKECANCEL        0  // Cancel transients/spikes/drops that have max duration of number chosen.
                              // Typically 4 or 8 to select at time periods of 4 or 8 times 2.9ms.
                              // 0 to deselect.

#define SHORTCANCEL        0  // Drops any transients (mark or space) that are shorter than 1/3rd "dot"
                              // length.  Only active when not in "initialize" state. Do not enable at
                              // same time as SPIKECANCEL. 1 to select, 0 to deselect.
                              
//-----------------------------------------------------------------------------  
// The below are to accomodate direct connection of a Morse Key, in addition to Audio input
#define LOCAL_KEY          1  // 1 for a Key being directly attached, else 0.
                              // This activates a keyed sine wave input.  Uses LOCAL_KEY input.
                              // No harm in having always enabled.

#define LOCAL_KEYFRQ     700  // Audio input frequency of Local Key (Hz).
#define KEY_LEVEL        0.6  // Level of sinewave input if LOCAL_KEY is enabled. 0.0 - 1.0

#define LOCAL_NOISE        0  // This activates a white noise source to simulate RX output for added
                              // realism if Audio from Radio is not connected. 1 to select, else 0.

#define NOISE_LEVEL      1.0  // Level of white noise if LOCAL_NOISE source is enabled. 0.0 - 1.0

//-----------------------------------------------------------------------------
// Decode of International Morse Code Symbols - a somewhat random collection of country specific symbols
// If you are not using these, then better not to enable.  The fewer unnecessary symbols - the more "meat"
// the Error Correction function gets to resolve
#define ICELAND_SYMBOLS    0  // Þ Ð Æ Ö
#define NOR_DEN_SYMBOLS    0  // Æ Å Ø   - Norway/Denmark - overlaps with Iceland symbols, only select one    
#define SWEDEN_SYMBOLS     0  // Ä Å Ö   - Sweden - overlaps with above symbols, only select one
#define REST_SYMBOLS       0  // Ü Ch É Ñ  - a random colletion of symbols

//-----------------------------------------------------------------------------
// LCD Type QDTech orST7735 
#define LCD_QDTECH         0  // 128x160 pixel LCD board using a Samsung S6D02A1 chip.
                              // Else 128x160 pixel LCD board using an ST7735 chip.
                              
#define ST7735_BLACKTAB    1  // If ST7735 then select one of these, depending on
#define ST7735_REDTAB      0  // whether the TFTs plastic wrap has a Red, Green or Black
#define ST7735_GREENTAB    0  // Tab when new.  Else, experiment - if the display has wrong
                              // colours or extra 'random' pixels on the top & left.
#define R_B_COLOUR_INVERT  1  // One 3rd party ST7735 display I have inverts the colours

//-----------------------------------------------------------------------------  
// Internal Workings - Interrupt time, Signal Input Buffer size, Data Buffer size
#define INTERRUPT_TIMER  200  // Interrupt timer, in microseconds
#define SIG_BUFSIZE      256  // Size of a circular buffer of decoded input levels and durations
#define DATA_BUFSIZE      40  // Size of a buffer of accumulated dot/dash information. Max is DATA_BUFSIZE-2
                              // Needs to be significantly longer than longest symbol 'sos'= ~30.

//-----------------------------------------------------------------------------
// Microcontroller Pin assignments  
#define KEY_PIN            8  // Input Pin for Locally connected CW key, if enabled
#define LED_PIN           20  // LED pin - Cannot be 13
#define VOL_PIN           15  // Volume adjust pin
#define TONE_PIN          17  // Tone adjust pin
#define THRESH_PIN        16  // Tone threshold level adjust pin 
#define SPD_RESET_PIN     10  // Pin to force reinitialization of CW Speed
#define FFT_SEL_PIN       12  // Pin to select between fft1024 and fft256
#define AUDIO_INPUT       AUDIO_INPUT_LINEIN // Audio input from Audio Shield
//
// SPI connections for Banggood 1.8" display
#define  SCLKPIN          14  // Alt HW SCLK - Audio shield uses the primary SPI pins
#define  MOSIPIN           7  // Alt HW MOSI - Audio shield uses the primary SPI pins
#define  CS_PIN            2
#define  DC_PIN            3
#define  RST_PIN           1
//
// Pins in use by the PJRC Audio shield:
// 9, 11 13, 18, 19, 22, 23
// Available pins: 
// 0, 4, 5, 6, 21


//Graham - FIXME - ick - we have 'bool' for that!
#define  TRUE  1
#define  FALSE 0
