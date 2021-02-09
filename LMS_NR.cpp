
#include <Audio.h>
#include <arm_math.h>
#include <arm_const_structs.h>

#include "LMS_NR.h"

#define MAX_LMS_TAPS    96
#define MAX_LMS_DELAY   256
float32_t DMAMEM LMS_errsig1[256 + 10];
arm_lms_norm_instance_f32  LMS_Norm_instance;
arm_lms_instance_f32      LMS_instance;
float32_t                 DMAMEM LMS_StateF32[MAX_LMS_TAPS + MAX_LMS_DELAY];
float32_t                 DMAMEM LMS_NormCoeff_f32[MAX_LMS_TAPS + MAX_LMS_DELAY];
float32_t                 DMAMEM LMS_nr_delay[512 + MAX_LMS_DELAY];
int                       LMS_nr_strength = 5;


void Init_LMS_NR ()
{
  // Initialize LMS (DSP Noise reduction) filter
  //
  uint16_t  calc_taps = 96;
  float32_t mu_calc;

  for(unsigned i = 0; i < (MAX_LMS_TAPS + MAX_LMS_DELAY); i++)
  {
      LMS_StateF32[i] = 0.0;
      LMS_NormCoeff_f32[i] = 0.0;
  }

  for(unsigned i = 0; i < (512 + MAX_LMS_DELAY); i++)
  {
      LMS_nr_delay[i] = 0.0;
  }

  LMS_Norm_instance.numTaps = calc_taps;
  LMS_Norm_instance.pCoeffs = LMS_NormCoeff_f32;
  LMS_Norm_instance.pState = LMS_StateF32;

  // Calculate "mu" (convergence rate) from user "DSP Strength" setting.  This needs to be significantly de-linearized to
  // squeeze a wide range of adjustment (e.g. several magnitudes) into a fairly small numerical range.
  mu_calc = LMS_nr_strength;   // get user setting

  // New DSP NR "mu" calculation method as of 0.0.214
  mu_calc /= 2; // scale input value
  mu_calc += 2; // offset zero value
  mu_calc /= 10;  // convert from "bels" to "deci-bels"
  mu_calc = powf(10, mu_calc);  // convert to ratio
  mu_calc = 1 / mu_calc;    // invert to fraction
  LMS_Norm_instance.mu = mu_calc;

  arm_fill_f32(0.0, LMS_nr_delay, 512 + 256);
  arm_fill_f32(0.0, LMS_StateF32, 96 + 256);

  // use "canned" init to initialize the filter coefficients
  arm_lms_norm_init_f32(&LMS_Norm_instance, calc_taps, &LMS_NormCoeff_f32[0], &LMS_StateF32[0], mu_calc, 256);

}

void LMS_NoiseReduction(int16_t blockSize, float32_t *nrbuffer)
{
  static ulong    lms1_inbuf = 0, lms1_outbuf = 0;

  arm_copy_f32(nrbuffer, &LMS_nr_delay[lms1_inbuf], blockSize);  // put new data into the delay buffer
  //
  arm_lms_norm_f32(&LMS_Norm_instance, nrbuffer, &LMS_nr_delay[lms1_outbuf], nrbuffer, LMS_errsig1, blockSize);  // do noise reduction
  //
  //
  lms1_inbuf += blockSize;  // bump input to the next location in our de-correlation buffer
  lms1_outbuf = lms1_inbuf + blockSize; // advance output to same distance ahead of input
  lms1_inbuf %= 512;
  lms1_outbuf %= 512;
}
