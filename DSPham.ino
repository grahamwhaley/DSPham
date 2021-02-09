
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <Encoder.h>

#include "global.h"
#include "menu.h"

#include "rgb_lcd.h"

#include "LMS_NR.h"
#include "nr_kim.h"
#include "fir.h"
#include "ik8yfw.h"
#include "spectral.h"
#include "nb.h"
#include "xanr.h"

#include "lcd.h"
#include "morseGen.h"
#include "morseDecode.h"
#include "k4icy.h"
#include "tf3lj.h"
#include "tf3lj_dec.h"
#include "dynamicFilters.h"
#include "dspfilter.h"

#include "settings.h"

//spectral stuff
extern float32_t tinc;
extern float32_t asnr;

Encoder enc1(3, 2);

AudioInputUSB            usb1;
AudioOutputI2S           i2s_out;
AudioInputI2S            i2s_in;

// Queues to allow us to grab the stream data, and then pass back out to play
//AudioRecordQueue Q_in_R;
AudioRecordQueue Q_in_L; // We only need one channel - we are working with mono right now
// I wonder if the I2S will keep feeding if we only consume a single channel??
AudioPlayQueue Q_out_R;
//AudioPlayQueue Q_out_L;

AudioAnalyzePeak input_peak_detector, output_peak_detector;

//RMS detection seems to work OK for auto output level matching, but
// then we don't get 'peak-o-meter' to show input levels...
//AudioAnalyzeRMS input_peak_detector, output_peak_detector;
AudioAmplifier peak_amp;

// Tone detector for morse decoding
AudioAnalyzeToneDetect toneDetect;
// And a note freq analyser to try and help narrow in on the signal...
AudioAnalyzeNoteFrequency noteFreq;

AudioFilterFIR firfilter;
AudioMixer4 input_mixer;

// Go with 256fft, as it can go 'faster' than fft1024, which limits us to
// they say 20wpm, which is no use ;-)
//For lofturj TF3LJ baysian morse decoder
AudioAnalyzeFFT256 morse_fft;

//For now just Mono inputs - later we may move to stereo, in which case we either
// set up parallel processing pipelines, or we mix here down to mono, but we'll have
// to set gains (likely 0.5) on each input channel so as not to saturate the output
AudioConnection          patchCord1(usb1, 0, input_mixer, 0);
AudioConnection          patchCord2(i2s_in, 0, input_mixer, 2);
AudioConnection          patchCord3(input_mixer, 0, firfilter, 0);
AudioConnection          patchCord4(firfilter, 0, Q_in_L, 0);
//gap - fill me or re-number sometime
AudioConnection          patchCord6(Q_out_R, 0, peak_amp, 0);
AudioConnection          patchCord7(peak_amp, 0, i2s_out, 0);
AudioConnection          patchCord8(peak_amp, 0, i2s_out, 1);
AudioConnection          patchCord9(input_mixer, 0, input_peak_detector, 0);
AudioConnection          patchCord10(peak_amp, 0, output_peak_detector, 0);
AudioConnection          patchCord11(Q_out_R, 0, toneDetect, 0);  //Should we do these after the peak amp?
AudioConnection          patchCord13(Q_out_R, 0, noteFreq, 0);    //Should we do these after the peak amp?

//The FFT might be expensive, and we may not be using it - we should probably put an 'amp switch' before it and
// turn it off when not in use.
AudioConnection          patchCord14(Q_out_R, 0, morse_fft, 0);    //Should we do these after the peak amp?

AudioControlSGTL5000     sgtl5000_1;

#define SAMPLE_RATE 44100.0

// number of FIR taps for decimation stages
// DF1 decimation factor for first stage
// DF2 decimation factor for second stage
// see Lyons 2011 chapter 10.2 for the theory
#define n_att  90.0
#define n_samplerate (SAMPLE_RATE/1000)
#define n_desired_BW 9.0
#define n_fstop ( (n_samplerate / DF) - n_desired_BW)
#define n_fpass (n_desired_BW / n_samplerate)

#define n_dec_taps  (1 + (n_att / (22.0 * (n_fstop - n_fpass))))

arm_fir_decimate_instance_f32 FIR_dec;
float32_t DMAMEM FIR_dec_coeffs[(uint16_t)n_dec_taps];
float32_t DMAMEM FIR_dec_state [(int)(n_dec_taps + BUFFER_SIZE * N_B - 1)];

float32_t DMAMEM FIR_int_coeffs[32];
arm_fir_interpolate_instance_f32 FIR_int;

#define INT_STATE_SIZE (24 + BUFFER_SIZE * N_B / (uint32_t)DF - 1)
float32_t DMAMEM FIR_int_state [INT_STATE_SIZE];

// How much gain to apply to try and match the output 'volume' to the original
// input volume.
float32_t peak_gain = 1.0;
float32_t input_peak = 1.0, output_peak = 1.0;   //Default start as the same
float32_t input_peak_acc = 0;
float32_t peak_ratio;
unsigned long peak_ticktime;
#define PEAK_MS_UPDATE  100 //Update 10 times a second

unsigned long display_update_deadline = 0;
#define DISPLAY_UPDATE_MS 250

//Tone detector - to give hints about morse
unsigned long tone_update_deadline = 0;
#define TONE_UPDATE_MS 250

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("Starting");
#endif

  init_settings();  //load the eeprom
  spectral_noise_reduction_init();
  Init_LMS_NR();
  nr_kim_init();
  xanr_init();
  AudioMemory(64);    //Lots - we have RAM to spare..
  input_mixer.gain(0, 1.0);
  input_mixer.gain(2, 1.0);
  sgtl5000_1.enable();
  load_volume();    //load volume from eeprom
  sgtl5000_1.lineInLevel(7);  //0.94v p-p
  sgtl5000_1.lineOutLevel(31);  //1.16v p-p

  noteFreq.begin(0.15);
  lcd_setup();
  load_colour();    //load lcd screen colour.
  morseInit();
  k4icy_setup();
  morse_fft.windowFunction(AudioWindowBlackmanNuttall256);
  morse_fft.averageTogether(FFTAVERAGE); // Average for spike/noise canceling - does nothing with FFT1024
  tf3lj_init();
  tf3lj_dec_init();
  menu_setup();

  // We should work out why the taps is hard wired to 32 (48) here?? How is/should that be calculated!
  // These are the FIR filters for the noise reduction
  calc_FIR_coeffs (FIR_int_coeffs, 32, (float32_t)(n_desired_BW * 1000.0), n_att, 0, 0.0, n_samplerate*1000);
  if (arm_fir_interpolate_init_f32(&FIR_int, (uint8_t)DF, 32, FIR_int_coeffs, FIR_int_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF))
  {
    lcd.print("INT coeff fail");
    while(1);
  }

  calc_FIR_coeffs (FIR_dec_coeffs, n_dec_taps, (float32_t)(n_desired_BW * 1000.0), n_att, 0, 0.0, SAMPLE_RATE);
  if (arm_fir_decimate_init_f32(&FIR_dec, n_dec_taps, (uint32_t)DF , FIR_dec_coeffs, FIR_dec_state, BUFFER_SIZE * N_BLOCKS))
  {
    lcd.print("DEC coeff fail");
    while(1);
  }

  current_filter_mode = 0;
  updateFilter();

  //And then load the default slot settings before we begin
  //This also ensures we call the correct init routines before launch
  //into processing.
  load_specific_settings(get_default_slot());

  Q_in_L.begin();
  peak_ticktime = millis();   //wait one period before starting to do peak analysis
}

void loop() {
  int16_t *inp;
  int16_t *outp;
  static int bufcount=0;
  static long enc1_change = 0;
  static long enc1_change_time = 0;
  unsigned long ms;
  static bool in_menu = false; //Track history to find mode transition
  static float oldvol = 0;

  display = !menu_poll();

  ms = millis();
  // read the PC's volume setting
  float vol = usb1.volume();

  unsigned long start_micros = 0;
  unsigned long ready_micros = 0;
  static unsigned long finished_micros = 0;
  static float32_t pc_used;

  // Do we have any volume setting from USB? If not, use our
  // menu global setting.
  // *BUG* - well, feature request - would be nice if we can tell from the
  // USB if it is connected or not...
  // Global volume will have been written to the sg5k at startup and on
  // menu value changes..
  if (vol == 0 ) {
    if (global_volume != oldvol) {
      sgtl5000_1.volume(global_volume);
      oldvol = global_volume;
    }
  } else {
    // scale to a nice range (not too loud)
    // and adjust the audio shield output volume
    if (vol != oldvol) {
      char buf[64];
      oldvol = vol;
      // scale 0 = 1.0 range to:
      //  0.3 = almost silent
      //  0.8 = really loud
      // Tweaked to allow full 1.0 setting - as 0.8 can still seem quiet.
      // But, it is noted in the docs that maybe >0.8 distorts.
      vol = 0.3 + vol * 0.7;
      // use the scaled volume setting.  Delete this for fixed volume.
      sgtl5000_1.volume(vol);
    }
  }

  // Read in N_BLOCKS at a time... we only care about the Left channel, as we are only mono mode right now.
  if (Q_in_L.available() >= N_BLOCKS )
  {
    //Note when enough data became ready
    ready_micros = micros();

    for (unsigned i = 0; i < N_BLOCKS; i++)
    {
      q15_t max_value;
      uint32_t max_index;
      // We only process mono audio at the moment, even if the i2s is running in stereo mode..
      inp = Q_in_L.readBuffer();
      arm_max_q15(inp, AUDIO_BLOCK_SAMPLES, &max_value, &max_index);

      arm_q15_to_float (inp, &float_buffer_L[i * AUDIO_BLOCK_SAMPLES], AUDIO_BLOCK_SAMPLES); // convert int_buffer to float 32bit
      Q_in_L.freeBuffer();
    }

    if (nr_mode != NR_MODE_COMPLETE_BYPASS ) {
      if (nb_enabled ) {
        float32_t *Energy = 0;
        
        alt_noise_blanking(float_buffer_L, AUDIO_BLOCK_SAMPLES * N_BLOCKS / DF, Energy);
        memcpy(float_buffer_R, float_buffer_L, sizeof(float32_t) * BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF));
      }
  
      if (xanr_notch) {
        //Reads from L, leaves result in R
        xanr(true);
        //Copy result back to L for any further processing
        memcpy(float_buffer_L, float_buffer_R, sizeof(float32_t) * BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF));
        //arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length / 2);
      }
  
      // No processing - straight copy over.
      if (nr_mode == NR_MODE_OFF )
      {
        memcpy(float_buffer_R, float_buffer_L, sizeof(float32_t) * BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF));
      }
  
      if (nr_mode == NR_MODE_KIM )
      {
        // Kim code reads in from L buffer. Leaves result in both L and R buffers.
        nr_kim();
      }
  
      if (nr_mode == NR_MODE_LMS )
      {
        LMS_NoiseReduction(AUDIO_BLOCK_SAMPLES * N_BLOCKS / DF, float_buffer_L);
        // And copy results out to play
        memcpy(float_buffer_R, float_buffer_L, sizeof(float32_t) * BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF));
      }
  
      if (nr_mode == NR_MODE_FNR )
      {
        for( int i=0; i<AUDIO_BLOCK_SAMPLES * N_BLOCKS / DF; i++ )
        {
          float32_t v;
          v = fnrFilter_n(float_buffer_L[i], fnr_level);
          float_buffer_R[i] = v;
        }
      }
  
      if (nr_mode == NR_MODE_FNRA )
      {
        for( int i=0; i<AUDIO_BLOCK_SAMPLES * N_BLOCKS / DF; i++ )
        {
          float32_t v;
          v = fnrFilter_n_Average(float_buffer_L[i], fnra_level);
          float_buffer_R[i] = v;
        }
      }
  
      if (nr_mode == NR_MODE_SPECTRAL )
      {
        // Reads input from L, leaves output in R and L
        spectral_noise_reduction();
      }
  
      if (nr_mode == NR_MODE_LLMS )
      {
        //Reads from L, puts result in R
        xanr(false);
        //Scale the result ... but why?
        arm_scale_f32(float_buffer_R, 4.0, float_buffer_R, FFT_length / 2);
      }
    } else {   //full bypass mode
      // Just copy over then.
      // Ideally we would not even do the float convert in full bypass mode... but, we don't currently keep
      // the non-float data around for that.
      memcpy(float_buffer_R, float_buffer_L, sizeof(float32_t) * BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF));
    }
    
    for (int i = 0; i < N_BLOCKS; i++)
    {
      outp = Q_out_R.getBuffer();
      while (outp == NULL)
      {
        delay(1);
        outp = Q_out_R.getBuffer();
      }
      // Finally back to 16bit samples...
      arm_float_to_q15 (&float_buffer_R[AUDIO_BLOCK_SAMPLES * i], outp, AUDIO_BLOCK_SAMPLES);
      Q_out_R.playBuffer(); // play it !
    }
    
    bufcount += N_BLOCKS;

    // Record when we last started (that is, last finished...), and
    // when we finished now...
    start_micros = finished_micros;
    finished_micros = micros();

    //We always evaluate the input peak so we can show the input level
    // on the status display.
    if (ms >= peak_ticktime ) { 
      peak_ticktime = ms + PEAK_MS_UPDATE;
      
      if( input_peak_detector.available() ) {
        input_peak = input_peak_detector.read();
        // Take a rolling average of the input peak for the 'peak' display.
        input_peak_acc = (input_peak_acc * 0.9) + input_peak;
      }

      // We only compare and calculate against the output peak if we are
      // in output tracking mode.
      if( (agc_mode == AGC_MODE_TRACK) && (nr_mode != NR_MODE_COMPLETE_BYPASS) ) {
        if( output_peak_detector.available() )
          output_peak = output_peak_detector.read();
    
        // How different are the input and output peaks?
        // Hmm, would it be better to use the RMS here, rather than the peak...
        // Consider if we have an impulse blanker for instance removing the peaks?
        peak_ratio = input_peak / output_peak;
    
        if (peak_ratio > 1.1 )
          //Output lower than input - adjust gain to 'catch up'
          peak_gain *= 1.1;
        
        if (peak_ratio < 0.9 )
          //Output higher than input - adjust gain to back off
          peak_gain *= 0.9;
    
        //Some startup conditions can lead us to race to an 'inf' gain or ratio, which we then
        // never really recover from. Clip the gain to some sensible multiply or divide by 5 ratio,
        // which seems sensible anyhow.
        if (peak_gain > 5.0) peak_gain = 5.0;
        if (peak_gain < 0.2) peak_gain = 0.2;
        
        // And adjust the output amp stage.
        peak_amp.gain(peak_gain);
      } else {
        // If we are not in peak track mode, ensure we set the peak gain amp to neutral passthrough
        peak_amp.gain(1.0);
      }
    }

    //You can read toneDetect as a bool entitiy
    if ( (decoder_mode == DECODER_MORSE) || (decoder_mode == DECODER_MORSE_K4ICY) || (decoder_mode == DECODER_MORSE_TF3LJ) ) {
      if (ms >= tone_update_deadline ) { 
        char buf[64];
        tone_update_deadline = ms + TONE_UPDATE_MS;

        if (noteFreq.available()) {
          float32_t freq = noteFreq.read();
          int freqdiff = (int)freq - morse_frequency;

          if ( abs(freqdiff) < morse_frequency/10 ) {
            //Tone pretty close
            lcd.createChar(7, morsechar2);  //'><'
          } else {
            if (freqdiff < 0 ) {
              //Tone too low
              if (freqdiff < -(morse_frequency/2) ) {
                lcd.createChar(7, morsechar0);  //'<<'
              } else {
                lcd.createChar(7, morsechar1);  //'<'
              }
            } else {
              //Tone too high
              if (freqdiff > morse_frequency/2 ) {
                lcd.createChar(7, morsechar3);  //'>>'
              } else {
                lcd.createChar(7, morsechar4);  //'>'
              }         
            }
          }
          //Char is printed in the global lcd update routine
        }
      }

      if (decoder_mode == DECODER_MORSE_TF3LJ ) {
        if( morse_fft.available() ) {
          tf3lj_process();  // Process the fft data
          //Graham - FIXME - these counters are probably wrong as we are not doing an
          // interrupt driven fft process...
          sig_incount = sig_lastrx;
          cur_time = sig_timer;
          CW_Decode();      // And then process any generated morse data
        }
      } else {
        //I tried to only do a key up/down on a tonedetect state change, that is,
        // only send an up/down when we are actually transitioning - but, the
        //k4icy decoder at least stopped decoding when I did this - so, let's
        // leave it as is - and send the key state per 'cycle', even if it has
        // not changed, and presume the decoders can handle this!
        if( toneDetect ){
          //Key is down!
          morseLed(true);
          if (decoder_mode == DECODER_MORSE ) morseKeyDown();
          if (decoder_mode == DECODER_MORSE_K4ICY ) k4icy_keyDown();
        } else {
          //key up!
          morseLed(false);
          if (decoder_mode == DECODER_MORSE) morseKeyUp();
          if (decoder_mode == DECODER_MORSE_K4ICY) k4icy_keyUp();
        }
      }
    }

    //Always calculate the CPU usage - otherwise we 'wrap' and the calc goes wrong.
    {
      // Ignoring the potential timer wrapping... about every 70 minutes?
      unsigned long micros_total = finished_micros - start_micros;
      unsigned long micros_used = finished_micros - ready_micros;
      pc_used = ((float32_t)micros_used / (float32_t)micros_total) * 100.0;
    }
  } // end of processing an audio block set

  if (display) {
    int enc_change;
    static unsigned long last_change = 0;
    const unsigned long debounce_gap = 250;  //ms

    enc_change = enc1.read();
    enc1.write(0);

    // If we are coming out of a menu transition then drop the encoder
    // counts that were added up during the menu access - they were not for
    // us...
    if (in_menu) {
      in_menu = false;
    } else {
      if (enc_change != 0 )
      {
        // Are these clicks too close to the last set - if so, drop them to 'debounce'
        // We could also use a state filter to do this as per https://www.best-microcontroller-projects.com/rotary-encoder.html
        // state=(state<<1) | digitalRead(CLK_PIN) | 0xe000;
        if (ms > last_change + debounce_gap ) {        
          if (enc_change > 0) {
            //nr_mode++;
            //if (nr_mode > NR_MODE_MAX) nr_mode = 0;
            load_next_settings();
          }
          
          if (enc_change < 0) {
            //nr_mode--;
            //if (nr_mode < 0) nr_mode = NR_MODE_MAX;
            load_previous_settings();
          }
        }
        last_change = ms;
      }
    }
  } else {
    in_menu = true;
  }
  
  //Refresh the display periodically
  if (ms > display_update_deadline ) {
    char idx;
    char buf[10];

    display_update_deadline = ms + DISPLAY_UPDATE_MS;

    //Do not display if the menu is active.
    if (display) {
      static char cpubuf[32];

      updateDisplay();

      //Limit to 99% - otherwise we overflow our display slot (even with %2f??)
      sprintf(cpubuf, "%2d%%", ((int)pc_used)%100 );
      lcd.setCursor(10, 1);
      lcd.print(cpubuf);
    }
  }

#if 0 //useful serial menu - useful for interim var tweaking during development
extern int ANR_taps;
extern int ANR_delay;
extern float32_t ANR_two_mu;
extern float32_t ANR_gamma;

  if (Serial.available() > 0 ) {
    char c = Serial.read();
    char buf[64];
    float32_t newtinc;
    float32_t newasnr;

    switch( c ) {
      case 'T':
        ANR_taps++;
        sprintf(buf, "Taps: %d\n", ANR_taps);
        Serial.print(buf);
        break;

      case 't':
        ANR_taps--;
        sprintf(buf, "Taps: %d\n", ANR_taps);
        Serial.print(buf);
        break;

      case 'D':
        ANR_delay += 8;
        sprintf(buf, "Delay: %d\n", ANR_delay);
        Serial.print(buf);
        break;

      case 'd':
        ANR_delay -= 8;
        sprintf(buf, "Delay: %d\n", ANR_delay);
        Serial.print(buf);
        break;

      case 'M':
        ANR_two_mu *= 2.0;
        sprintf(buf, "Mu: %f\n", ANR_two_mu);
        Serial.print(buf);
        break;

      case 'm':
        ANR_two_mu /= 2.0;
        sprintf(buf, "Mu: %f\n", ANR_two_mu);
        Serial.print(buf);
        break;

      case 'G':
        ANR_gamma *= 2.0;
        sprintf(buf, "Gamma: %f\n", ANR_gamma);
        Serial.print(buf);
        break;

      case 'g':
        ANR_gamma /= 2.0;
        sprintf(buf, "Gamma: %f\n", ANR_gamma);
        Serial.print(buf);
        break;

    }
  }
#endif  //Serial
}
