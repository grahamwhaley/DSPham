#include <Audio.h>
#include <arm_math.h>
#include <arm_const_structs.h>

#include "global.h"
#include "nr_kim.h"

//global float32_t DMAMEM NR_FFT_buffer [512] __attribute__ ((aligned (4)));
//global const static arm_cfft_instance_f32 *NR_FFT;

//global float32_t DMAMEM NR_last_sample_buffer_L [NR_FFT_L / 2];
//global float32_t DMAMEM NR_X[NR_FFT_L / 2][3]; // magnitudes (fabs) of the last four values of FFT results for 128 frequency bins
uint32_t NR_X_pointer = 0;
float32_t NR_sum = 0.0;
const uint8_t NR_L_frames = 3; // default 3 //4 //3//2 //4
//global const uint8_t NR_N_frames = 15; // default 24 //40 //12 //20 //18//12 //20
//global float32_t DMAMEM NR_E[NR_FFT_L / 2][NR_N_frames]; // averaged (over the last four values) X values for the last 20 FFT frames
uint32_t NR_E_pointer = 0;
//global float32_t DMAMEM NR_M[NR_FFT_L / 2]; // minimum of the 20 last values of E
float32_t NR_T;
float32_t NR_PSI = 3.0; // default 3.0, range of 2.5 - 3.5 ?; 6.0 leads to strong reverb effects
float32_t DMAMEM NR_lambda[NR_FFT_L / 2]; // SNR of each current bin
uint8_t NR_use_X = 0;
//global float32_t DMAMEM NR_G[NR_FFT_L / 2]; // preliminary gain factors (before time smoothing) and after that contains the frequency smoothed gain factors
float32_t NR_KIM_K = 1.0; // K is the strength of the KIm & Ruwisch noise reduction
//global float32_t DMAMEM NR_Gts[NR_FFT_L / 2][2]; // time smoothed gain factors (current and last) for each of the 128 bins
//global float32_t NR_alpha = 0.95; // default 0.99 --> range 0.98 - 0.9999; 0.95 acts much too hard: reverb effects
float32_t NR_onemalpha = (1.0 - NR_alpha);
float32_t NR_beta = 0.85;
float32_t NR_onemtwobeta = (1.0 - (2.0 * NR_beta));
//global const static arm_cfft_instance_f32 *NR_iFFT;
float32_t DMAMEM NR_output_audio_buffer [NR_FFT_L];
//global float32_t DMAMEM NR_last_iFFT_result [NR_FFT_L / 2];

void nr_kim_init()
{
  NR_FFT = &arm_cfft_sR_f32_len256;
  NR_iFFT = &arm_cfft_sR_f32_len256;

  for (unsigned i = 0; i < NR_FFT_L; i++)
  {
      NR_FFT_buffer[i] = 0.0;
      NR_output_audio_buffer[i] = 0.0;
  }
  for (unsigned i = 0; i < NR_FFT_L / 2; i++)
  {
      NR_last_iFFT_result[i] = 0.0;
      NR_last_sample_buffer_L [i] = 0.0;
      NR_M[i] = 0.0;
      NR_lambda[i] = 0.0;
      NR_G[i] = 0.0;
  }

  for (unsigned j = 0; j < 3; j++)
  {
      for(unsigned i=0; i<NR_FFT_L / 2; i++)
      {
          NR_X[i][j] = 0.0;
      }
  }

  for (unsigned j = 0; j < 2; j++)
  {
      for(unsigned i=0; i<NR_FFT_L / 2; i++)
      {
          NR_Gts[i][j] = 0.0;
      }
  }

  for (unsigned j = 0; j < NR_N_frames; j++)
  {
      for(unsigned i=0; i<NR_FFT_L / 2; i++)
      {
          NR_E[i][j] = 0.0;
      }
  }

}

void nr_kim()
{
  ////////////////////////////////////////////////////////////////////////////////////////////////////////
  // this is exactly the implementation by
  // Kim & Ruwisch 2002 - 7th International Conference on Spoken Language Processing Denver, Colorado, USA
  // with two exceptions:
  // 1.) we use power instead of magnitude for X
  // 2.) we need to clamp for negative gains . . .
  ////////////////////////////////////////////////////////////////////////////////////////////////////////

  // perform a loop two times (each time process 128 new samples)
  // FFT 256 points
  // frame step 128 samples
  // half-overlapped data buffers

  uint8_t VAD_low = 0;
  uint8_t VAD_high = 127;
  float32_t lf_freq; // = (offset - width/2) / (12000 / NR_FFT_L); // bin BW is 46.9Hz [12000Hz / 256 bins] @96kHz
  float32_t uf_freq;

  lf_freq = 100;
  uf_freq = 3600;

  //Our sample rate is currently fixed - no need to use a lookup table.
  lf_freq /= (SAMPLE_RATE / DF) / NR_FFT_L;
  uf_freq /= (SAMPLE_RATE / DF) / NR_FFT_L;

  VAD_low = (int)lf_freq;
  VAD_high = (int)uf_freq;
  if (VAD_low == VAD_high)
  {
    VAD_high++;
  }
  if (VAD_low < 1)
  {
    VAD_low = 1;
  }
  else if (VAD_low > NR_FFT_L / 2 - 2)
  {
    VAD_low = NR_FFT_L / 2 - 2;
  }
  if (VAD_high < 1)
  {
    VAD_high = 1;
  }
  else if (VAD_high > NR_FFT_L / 2)
  {
    VAD_high = NR_FFT_L / 2;
  }

  //Graham - hack
  //VAD_low = 1;
  //VAD_high = NR_FFT_L / 2;

  for (int k = 0; k < 2; k++)
  {
    // NR_FFT_buffer is 512 floats big
    // interleaved r, i, r, i . . .

    // fill first half of FFT_buffer with last events audio samples
    for (int i = 0; i < NR_FFT_L / 2; i++)
    {
      NR_FFT_buffer[i * 2] = NR_last_sample_buffer_L[i]; // real
      NR_FFT_buffer[i * 2 + 1] = 0.0; // imaginary
    }

    // copy recent samples to last_sample_buffer for next time!
    for (int i = 0; i < NR_FFT_L  / 2; i++)
    {
      NR_last_sample_buffer_L [i] = float_buffer_L[i + k * (NR_FFT_L / 2)];
    }

    // now fill recent audio samples into second half of FFT_buffer
    for (int i = 0; i < NR_FFT_L / 2; i++)
    {
      NR_FFT_buffer[NR_FFT_L + i * 2] = float_buffer_L[i + k * (NR_FFT_L / 2)]; // real
      NR_FFT_buffer[NR_FFT_L + i * 2 + 1] = 0.0;
    }

    /////////////////////////////////
    // WINDOWING
#if 1
    // perform windowing on 256 real samples in the NR_FFT_buffer
    for (int idx = 0; idx < NR_FFT_L; idx++)
    { // Hann window
      float32_t temp_sample = 0.5 * (float32_t)(1.0 - (cosf(PI * 2.0 * (float32_t)idx / (float32_t)((NR_FFT_L) - 1))));
      NR_FFT_buffer[idx * 2] *= temp_sample;
    }
#endif

#if 0

    // perform windowing on 256 real samples in the NR_FFT_buffer
    for (int idx = 0; idx < NR_FFT_L; idx++)
    { // sqrt Hann window
      NR_FFT_buffer[idx * 2] *= sqrtHann[idx];
    }
#endif

    // NR_FFT 256
    // calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
    arm_cfft_f32(NR_FFT, NR_FFT_buffer, 0, 1);

    // pass-thru
    //    arm_copy_f32(NR_FFT_buffer, NR_iFFT_buffer, NR_FFT_L * 2);

    /******************************************************************************************************
        Noise reduction starts here

        PROBLEM: negative gain values results!
     ******************************************************************************************************/

    // for debugging
    //  for(int idx = 0; idx < 5; idx++)
    //  {
    //      Serial.println(NR_FFT_buffer[idx]);
    //  }
    // the buffer contents are negative and positive, so taking absolute values for magnitude detection does seem to make some sense ;-)

    // NR_FFT_buffer contains interleaved 256 real and 256 imaginary values {r, i, r, i, r, i, . . .}
    // as far as I know, the first 128 contain the real part of the respective channel, maybe I am wrong???

    // the strategy is to take ONLY the real values (one channel) to estimate the noise reduction gain factors (in order to save processor time)
    // and then apply the same gain factors to both channel
    // we will see (better: hear) whether that makes sense or not

    // 2. MAGNITUDE CALCULATION  we save the absolute values of the bin results (bin magnitudes) in an array of 128 x 4 results in time [float32_t
    // [BTW: could we subsititue this step with a simple one pole IIR ?]
    // NR_X [128][4] contains the bin magnitudes
    // 2a copy current results into NR_X

    for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // take first 128 bin values of the FFT result
    { // it seems that taking power works better than taking magnitude . . . !?
      //NR_X[bindx][NR_X_pointer] = sqrtf(NR_FFT_buffer[bindx * 2] * NR_FFT_buffer[bindx * 2] + NR_FFT_buffer[bindx * 2 + 1] * NR_FFT_buffer[bindx * 2 + 1]);
      NR_X[bindx][NR_X_pointer] = (NR_FFT_buffer[bindx * 2] * NR_FFT_buffer[bindx * 2] + NR_FFT_buffer[bindx * 2 + 1] * NR_FFT_buffer[bindx * 2 + 1]);
    }

    // 3. AVERAGING: We average over these L_frames (eg. 4) results (for every bin) and save the result in float32_t NR_E[128, 20]:
    //    we do this for the last 20 averaged results.
    // 3a calculate average of the four values and save in E

    //            for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // take first 128 bin values of the FFT result
    for (int bindx = VAD_low; bindx < VAD_high; bindx++) // take first 128 bin values of the FFT result
    {
      NR_sum = 0.0;
      for (int j = 0; j < NR_L_frames; j++)
      { // sum up the L_frames |X|
        NR_sum = NR_sum + NR_X[bindx][j];
      }
      // divide sum of L_frames |X| by L_frames to calculate the average and save in NR_E
      NR_E[bindx][NR_E_pointer] = NR_sum / (float32_t)NR_L_frames;
    }

    // 4.  MINIMUM DETECTION: We search for the minimum in the last N_frames (eg. 20) results for E and save this minimum (for every bin): float32_t M[128]
    // 4a minimum search in all E values and save in M

    //            for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // take first 128 bin values of the FFT result
    for (int bindx = VAD_low; bindx < VAD_high; bindx++) // take first 128 bin values of the FFT result
    {
      // we have to reset the minimum value to the first E value every time we start with a bin
      NR_M[bindx] = NR_E[bindx][0];
      // therefore we start with the second E value (index j == 1)
      for (uint8_t j = 1; j < NR_N_frames; j++)
      { //
        if (NR_E[bindx][j] < NR_M[bindx])
        {
          NR_M[bindx] = NR_E[bindx][j];
        }
      }
    }
    ////////////////////////////////////////////////////
    // TODO: make min-search more efficient
    ////////////////////////////////////////////////////

    // 5.  SNR CALCULATION: We calculate the signal-noise-ratio of the current frame T = X / M for every bin. If T > PSI {lambda = M}
    //     else {lambda = E} (float32_t lambda [128])

    //            for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // take first 128 bin values of the FFT result
    for (int bindx = VAD_low; bindx < VAD_high; bindx++) // take first 128 bin values of the FFT result
    {
      NR_T = NR_X[bindx][NR_X_pointer] / NR_M[bindx]; // dies scheint mir besser zu funktionieren !
      if (NR_T > NR_PSI)
      {
        NR_lambda[bindx] = NR_M[bindx];
      }
      else
      {
        NR_lambda[bindx] = NR_E[bindx][NR_E_pointer];
      }
    }

#if DEBUG
    // for debugging
    for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++)
    {
      Serial.print((NR_lambda[bindx]), 6);
      Serial.print("   ");
    }
    Serial.println("-------------------------");
#endif

    // lambda is always positive
    // > 1 for bin 0 and bin 1, decreasing with bin number

    // 6.  SMOOTHED GAIN COMPUTATION: Calculate time smoothed gain factors float32_t Gts [128, 2],
    //     float32_t G[128]: G = 1 – (lambda / X); apply temporal smoothing: Gts (f, 0) = alpha * Gts (f, 1) + (1 – alpha) * G(f)

    //            for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // take first 128 bin values of the FFT result
    for (int bindx = VAD_low; bindx < VAD_high; bindx++) // take first 128 bin values of the FFT result
    {
      // the original equation is dividing by X. But this leads to negative gain factors sometimes!
      // better divide by E ???
      // we could also set NR_G to zero if its negative . . .

      if (NR_use_X)
      {
        NR_G[bindx] = 1.0 - (NR_lambda[bindx] * NR_KIM_K / NR_X[bindx][NR_X_pointer]);
        if (NR_G[bindx] < 0.0) NR_G[bindx] = 0.0;
      }
      else
      {
        NR_G[bindx] = 1.0 - (NR_lambda[bindx] * NR_KIM_K / NR_E[bindx][NR_E_pointer]);
        if (NR_G[bindx] < 0.0) NR_G[bindx] = 0.0;
      }

      // time smoothing
      NR_Gts[bindx][0] = NR_alpha * NR_Gts[bindx][1] + (NR_onemalpha) * NR_G[bindx];
      NR_Gts[bindx][1] = NR_Gts[bindx][0]; // copy for next FFT frame
    }

    // NR_G is always positive, however often 0.0

    // for debugging
#if DEBUG
    for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++)
    {
      Serial.print((NR_Gts[bindx][0]), 6);
      Serial.print("   ");
    }
    Serial.println("-------------------------");
#endif

    // NR_Gts is always positive, bin 0 and bin 1 large, about 1.2 to 1.5, all other bins close to 0.2

    // 7.  Frequency smoothing of gain factors (recycle G array): G (f) = beta * Gts(f-1,0) + (1 – 2*beta) * Gts(f , 0) + beta * Gts(f + 1,0)

    for (int bindx = 1; bindx < ((NR_FFT_L / 2) - 1); bindx++) // take first 128 bin values of the FFT result
    {
      NR_G[bindx] = NR_beta * NR_Gts[bindx - 1][0] + NR_onemtwobeta * NR_Gts[bindx][0] + NR_beta * NR_Gts[bindx + 1][0];
    }
    // take care of bin 0 and bin NR_FFT_L/2 - 1
    NR_G[0] = (NR_onemtwobeta + NR_beta) * NR_Gts[0][0] + NR_beta * NR_Gts[1][0];
    NR_G[(NR_FFT_L / 2) - 1] = NR_beta * NR_Gts[(NR_FFT_L / 2) - 2][0] + (NR_onemtwobeta + NR_beta) * NR_Gts[(NR_FFT_L / 2) - 1][0];


    //old, probably right
    //          NR_G[0] = NR_beta * NR_G_bin_m_1 + NR_onemtwobeta * NR_Gts[0][0] + NR_beta * NR_Gts[1][0];
    //          NR_G[NR_FFT_L / 2 - 1] = NR_beta * NR_Gts[NR_FFT_L / 2 - 2][0] + (NR_onemtwobeta + NR_beta) * NR_Gts[NR_FFT_L / 2 - 1][0];
    // save gain for bin 0 for next frame
    //          NR_G_bin_m_1 = NR_Gts[NR_FFT_L / 2 - 1][0];

    // for debugging
#if DEBUG
    for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++)
    {
      Serial.print((NR_G[bindx]), 6);
      Serial.print("   ");
    }
    Serial.println("-------------------------");
#endif

    // 8.  SPECTRAL WEIGHTING: Multiply current FFT results with NR_FFT_buffer for 256 bins with the 256 bin-specific gain factors G

    for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // try 128:
    {
      NR_FFT_buffer[bindx * 2] = NR_FFT_buffer [bindx * 2] * NR_G[bindx]; // real part
      NR_FFT_buffer[bindx * 2 + 1] = NR_FFT_buffer [bindx * 2 + 1] * NR_G[bindx]; // imag part
      NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 2] = NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 2] * NR_G[bindx]; // real part conjugate symmetric
      NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 1] = NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 1] * NR_G[bindx]; // imag part conjugate symmetric
    }

    // DEBUG
#if DEBUG
    for (int bindx = 20; bindx < 21; bindx++)
    {
      Serial.println("************************************************");
      Serial.print("E: "); Serial.println(NR_E[bindx][NR_E_pointer]);
      Serial.print("MIN: "); Serial.println(NR_M[bindx]);
      Serial.print("lambda: "); Serial.println(NR_lambda[bindx]);
      Serial.print("X: "); Serial.println(NR_X[bindx][NR_X_pointer]);
      Serial.print("lanbda / X: "); Serial.println(NR_lambda[bindx] / NR_X[bindx][NR_X_pointer]);
      Serial.print("Gts: "); Serial.println(NR_Gts[bindx][0]);
      Serial.print("Gts old: "); Serial.println(NR_Gts[bindx][1]);
      Serial.print("Gfs: "); Serial.println(NR_G[bindx]);
    }
#endif

    // increment pointer AFTER everything has been processed !
    // 2b ++NR_X_pointer --> increment pointer for next FFT frame
    NR_X_pointer = NR_X_pointer + 1;
    if (NR_X_pointer >= NR_L_frames)
    {
      NR_X_pointer = 0;
    }
    // 3b ++NR_E_pointer
    NR_E_pointer = NR_E_pointer + 1;
    if (NR_E_pointer >= NR_N_frames)
    {
      NR_E_pointer = 0;
    }


#if 0
    for (int idx = 1; idx < 20; idx++)
      // bins 2 to 29 attenuated
      // set real values to 0.1 of their original value
    {
      NR_iFFT_buffer[idx * 2] *= 0.1;
      NR_iFFT_buffer[NR_FFT_L * 2 - ((idx + 1) * 2)] *= 0.1; //NR_iFFT_buffer[idx] * 0.1;
      NR_iFFT_buffer[idx * 2 + 1] *= 0.1; //NR_iFFT_buffer[idx] * 0.1;
      NR_iFFT_buffer[NR_FFT_L * 2 - ((idx + 1) * 2) + 1] *= 0.1; //NR_iFFT_buffer[idx] * 0.1;
    }
#endif

    // NR_iFFT
    // perform iFFT (in-place)
    arm_cfft_f32(NR_iFFT, NR_FFT_buffer, 1, 1);

#if 0
    // perform windowing on 256 real samples in the NR_FFT_buffer
    for (int idx = 0; idx < NR_FFT_L; idx++)
    { // sqrt Hann window
      NR_FFT_buffer[idx * 2] *= sqrtHann[idx];
    }
#endif

    // do the overlap & add

    for (int i = 0; i < NR_FFT_L / 2; i++)
    { // take real part of first half of current iFFT result and add to 2nd half of last iFFT_result
      NR_output_audio_buffer[i + k * (NR_FFT_L / 2)] = NR_FFT_buffer[i * 2] + NR_last_iFFT_result[i];
    }

    for (int i = 0; i < NR_FFT_L / 2; i++)
    {
      NR_last_iFFT_result[i] = NR_FFT_buffer[NR_FFT_L + i * 2];
    }

    // end of "for" loop which repeats the FFT_iFFT_chain two times !!!
  }

  for (int i = 0; i < NR_FFT_L; i++)
  {
    float_buffer_L [i] = NR_output_audio_buffer[i]; // * 9.0; // * 5.0;
    float_buffer_R [i] = float_buffer_L [i];
  }
} // end of Kim et al. 2002 algorithm
