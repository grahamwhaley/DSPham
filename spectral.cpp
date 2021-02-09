#include <Audio.h>
#include <arm_math.h>
#include <arm_const_structs.h>

#include "global.h"
#include "spectral.h"

float32_t tinc = 0.00145; // frame time 5.3333ms
float32_t asnr = 20;  // active SNR in dB

// spectral weighting noise reduction
// based on:
// Kim, H.-G. & D. Ruwisch (2002): Speech enhancement in non-stationary noise environments. – 7th International Conference on Spoken Language Processing [ICSLP 2002]. – ISCA Archive (http://www.isca-speech.org/archive)

// spectral specific vars - don't need to be global (yet).
const static arm_cfft_instance_f32 *S;
const static arm_cfft_instance_f32 *iS;
const static arm_cfft_instance_f32 *maskS;
const static arm_cfft_instance_f32 *spec_FFT;
float32_t DMAMEM NR_Hk_old[NR_FFT_L / 2];
float32_t DMAMEM NR_Nest[NR_FFT_L / 2][2]; //
float32_t DMAMEM NR_SNR_post[NR_FFT_L / 2];
float32_t DMAMEM NR_SNR_prio[NR_FFT_L / 2];
uint8_t NR_first_time = 1;
float32_t DMAMEM NR_long_tone_gain[NR_FFT_L / 2];
const float32_t sqrtHann[256] = {0, 0.01231966, 0.024637449, 0.036951499, 0.049259941, 0.061560906, 0.073852527, 0.086132939, 0.098400278, 0.110652682, 0.122888291, 0.135105247, 0.147301698, 0.159475791, 0.171625679, 0.183749518, 0.195845467, 0.207911691, 0.219946358, 0.231947641, 0.24391372, 0.255842778, 0.267733003, 0.279582593, 0.291389747, 0.303152674, 0.314869589, 0.326538713, 0.338158275, 0.349726511, 0.361241666, 0.372701992, 0.384105749, 0.395451207, 0.406736643, 0.417960345, 0.429120609, 0.440215741, 0.451244057, 0.462203884, 0.473093557, 0.483911424, 0.494655843, 0.505325184, 0.515917826, 0.526432163, 0.536866598, 0.547219547, 0.557489439, 0.567674716, 0.577773831, 0.587785252, 0.597707459, 0.607538946, 0.617278221, 0.626923806, 0.636474236, 0.645928062, 0.65528385, 0.664540179, 0.673695644, 0.682748855, 0.691698439, 0.700543038, 0.709281308, 0.717911923, 0.726433574, 0.734844967, 0.743144825, 0.75133189, 0.759404917, 0.767362681, 0.775203976, 0.78292761, 0.790532412, 0.798017227, 0.805380919, 0.812622371, 0.819740483, 0.826734175, 0.833602385, 0.840344072, 0.846958211, 0.853443799, 0.859799851, 0.866025404, 0.872119511, 0.878081248, 0.88390971, 0.889604013, 0.895163291, 0.900586702, 0.905873422, 0.911022649, 0.916033601, 0.920905518, 0.92563766, 0.930229309, 0.934679767, 0.938988361, 0.943154434, 0.947177357, 0.951056516, 0.954791325, 0.958381215, 0.961825643, 0.965124085, 0.968276041, 0.971281032, 0.974138602, 0.976848318, 0.979409768, 0.981822563, 0.984086337, 0.986200747, 0.988165472, 0.989980213, 0.991644696, 0.993158666, 0.994521895, 0.995734176, 0.996795325, 0.99770518, 0.998463604, 0.999070481, 0.99952572, 0.99982925, 0.999981027, 0.999981027, 0.99982925, 0.99952572, 0.999070481, 0.998463604, 0.99770518, 0.996795325, 0.995734176, 0.994521895, 0.993158666, 0.991644696, 0.989980213, 0.988165472, 0.986200747, 0.984086337, 0.981822563, 0.979409768, 0.976848318, 0.974138602, 0.971281032, 0.968276041, 0.965124085, 0.961825643, 0.958381215, 0.954791325, 0.951056516, 0.947177357, 0.943154434, 0.938988361, 0.934679767, 0.930229309, 0.92563766, 0.920905518, 0.916033601, 0.911022649, 0.905873422, 0.900586702, 0.895163291, 0.889604013, 0.88390971, 0.878081248, 0.872119511, 0.866025404, 0.859799851, 0.853443799, 0.846958211, 0.840344072, 0.833602385, 0.826734175, 0.819740483, 0.812622371, 0.805380919, 0.798017227, 0.790532412, 0.78292761, 0.775203976, 0.767362681, 0.759404917, 0.75133189, 0.743144825, 0.734844967, 0.726433574, 0.717911923, 0.709281308, 0.700543038, 0.691698439, 0.682748855, 0.673695644, 0.664540179, 0.65528385, 0.645928062, 0.636474236, 0.626923806, 0.617278221, 0.607538946, 0.597707459, 0.587785252, 0.577773831, 0.567674716, 0.557489439, 0.547219547, 0.536866598, 0.526432163, 0.515917826, 0.505325184, 0.494655843, 0.483911424, 0.473093557, 0.462203884, 0.451244057, 0.440215741, 0.429120609, 0.417960345, 0.406736643, 0.395451207, 0.384105749, 0.372701992, 0.361241666, 0.349726511, 0.338158275, 0.326538713, 0.314869589, 0.303152674, 0.291389747, 0.279582593, 0.267733003, 0.255842778, 0.24391372, 0.231947641, 0.219946358, 0.207911691, 0.195845467, 0.183749518, 0.171625679, 0.159475791, 0.147301698, 0.135105247, 0.122888291, 0.110652682, 0.098400278, 0.086132939, 0.073852527, 0.061560906, 0.049259941, 0.036951499, 0.024637449, 0.01231966, 0};

void spectral_noise_reduction_init()
{

  // Init code imported from main init routine.
  /****************************************************************************************
     init complex FFTs
  ****************************************************************************************/
  switch (FFT_length)
  {
    case 2048:
      S = &arm_cfft_sR_f32_len2048;
      iS = &arm_cfft_sR_f32_len2048;
      maskS = &arm_cfft_sR_f32_len2048;
      break;
    case 1024:
      S = &arm_cfft_sR_f32_len1024;
      iS = &arm_cfft_sR_f32_len1024;
      maskS = &arm_cfft_sR_f32_len1024;
      break;
    case 512:
      S = &arm_cfft_sR_f32_len512;
      iS = &arm_cfft_sR_f32_len512;
      maskS = &arm_cfft_sR_f32_len512;
      break;
  }

  spec_FFT = &arm_cfft_sR_f32_len256;
  NR_FFT = &arm_cfft_sR_f32_len256;
  NR_iFFT = &arm_cfft_sR_f32_len256;
  
    
  for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++)
  {
    NR_last_sample_buffer_L[bindx] = 0.1;
    NR_Hk_old[bindx] = 0.1; // old gain
    NR_Nest[bindx][0] = 0.01;
    NR_Nest[bindx][1] = 0.015;
    NR_Gts[bindx][1] = 0.1;
    NR_M[bindx] = 500.0;
    NR_E[bindx][0] = 0.1;
    NR_X[bindx][1] = 0.5;
    NR_SNR_post[bindx] = 2.0;
    NR_SNR_prio[bindx] = 1.0;
    NR_first_time = 2;
    NR_long_tone_gain[bindx] = 1.0;
  }

  //These bits taken from the Convolution 'DMA var' init code - they should probably really be in a
  //global 'NR' init section - a bunch is shared between Kim and spectral.
  for (unsigned i = 0; i < NR_FFT_L / 2; i++)
  {
      NR_last_iFFT_result[i] = 0.0;
      NR_last_sample_buffer_L [i] = 0.0;
      NR_M[i] = 0.0;
      NR_G[i] = 0.0;
      NR_SNR_prio[i] = 0.0;
      NR_SNR_post[i] = 0.0;
      NR_Hk_old[i] = 0.0;
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
          NR_Nest[i][j] = 0.0;
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

#if 0 //uncomment if/when we get xanr back
  for(unsigned i = 0; i < ANR_DLINE_SIZE; i++)
  {
      ANR_d[i] = 0.0;
      ANR_w[i] = 0.0;
  }
#endif
}

void spectral_noise_reduction (void)
/************************************************************************************************************

      Noise reduction with spectral subtraction rule
      based on Romanin et al. 2009 & Schmitt et al. 2002
      and MATLAB voicebox
      and Gerkmann & Hendriks 2002
      and Yao et al. 2016

   STAND: UHSDR github 14.1.2018
   ************************************************************************************************************/
{
  //const float32_t sqrtHann = {0,0.006147892,0.012295552,0.018442747,0.024589245,0.030734813,0.03687922,0.043022233,0.04916362,0.055303148,0.061440587,0.067575703,0.073708265,0.07983804,0.085964799,0.092088308,0.098208336,0.104324653,0.110437026,0.116545225,0.122649019,0.128748177,0.134842469,0.140931665,0.147015533,0.153093845,0.159166371,0.16523288,0.171293144,0.177346934,0.18339402,0.189434175,0.19546717,0.201492777,0.207510768,0.213520915,0.219522993,0.225516773,0.231502029,0.237478535,0.243446065,0.249404393,0.255353295,0.261292545,0.26722192,0.273141194,0.279050144,0.284948547,0.290836179,0.296712819,0.302578244,0.308432233,0.314274564,0.320105016,0.325923369,0.331729404,0.3375229,0.343303638,0.349071401,0.35482597,0.360567128,0.366294657,0.372008341,0.377707965,0.383393313,0.389064169,0.39472032,0.400361552,0.405987651,0.411598406,0.417193603,0.422773031,0.42833648,0.433883739,0.439414599,0.44492885,0.450426284,0.455906694,0.461369871,0.46681561,0.472243705,0.477653951,0.483046143,0.488420077,0.49377555,0.49911236,0.504430306,0.509729185,0.515008798,0.520268945,0.525509428,0.530730048,0.535930608,0.541110912,0.546270763,0.551409967,0.556528329,0.561625657,0.566701756,0.571756436,0.576789506,0.581800774,0.586790052,0.591757151,0.596701884,0.601624063,0.606523503,0.611400018,0.616253424,0.621083537,0.625890175,0.630673157,0.635432301,0.640167428,0.644878358,0.649564914,0.654226918,0.658864195,0.663476568,0.668063864,0.67262591,0.677162532,0.681673559,0.686158822,0.690618149,0.695051373,0.699458327,0.703838843,0.708192756,0.712519902,0.716820117,0.721093238,0.725339104,0.729557554,0.733748429,0.737911571,0.742046822,0.746154026,0.750233028,0.754283673,0.758305808,0.762299282,0.766263944,0.770199643,0.77410623,0.777983559,0.781831482,0.785649855,0.789438533,0.793197372,0.79692623,0.800624968,0.804293444,0.80793152,0.811539059,0.815115924,0.818661981,0.822177094,0.825661132,0.829113962,0.832535454,0.835925479,0.839283909,0.842610616,0.845905475,0.849168362,0.852399152,0.855597725,0.858763958,0.861897733,0.864998931,0.868067434,0.871103127,0.874105896,0.877075625,0.880012204,0.882915521,0.885785467,0.888621932,0.891424811,0.894193996,0.896929383,0.89963087,0.902298353,0.904931732,0.907530907,0.91009578,0.912626255,0.915122235,0.917583626,0.920010335,0.922402271,0.924759343,0.927081462,0.92936854,0.931620491,0.933837229,0.936018671,0.938164734,0.940275338,0.942350402,0.944389848,0.9463936,0.94836158,0.950293715,0.952189932,0.954050159,0.955874327,0.957662364,0.959414206,0.961129784,0.962809034,0.964451894,0.9660583,0.967628192,0.96916151,0.970658197,0.972118197,0.973541453,0.974927912,0.976277522,0.977590232,0.978865992,0.980104754,0.98130647,0.982471097,0.983598589,0.984688904,0.985742,0.986757839,0.987736381,0.98867759,0.98958143,0.990447867,0.991276868,0.992068401,0.992822438,0.993538949,0.994217907,0.994859287,0.995463064,0.996029215,0.99655772,0.997048558,0.997501711,0.997917161,0.998294893,0.998634892,0.998937146,0.999201643,0.999428374,0.999617329,0.999768502,0.999881887,0.999957479,0.999995275,0.999995275,0.999957479,0.999881887,0.999768502,0.999617329,0.999428374,0.999201643,0.998937146,0.998634892,0.998294893,0.997917161,0.997501711,0.997048558,0.99655772,0.996029215,0.995463064,0.994859287,0.994217907,0.993538949,0.992822438,0.992068401,0.991276868,0.990447867,0.98958143,0.98867759,0.987736381,0.986757839,0.985742,0.984688904,0.983598589,0.982471097,0.98130647,0.980104754,0.978865992,0.977590232,0.976277522,0.974927912,0.973541453,0.972118197,0.970658197,0.96916151,0.967628192,0.9660583,0.964451894,0.962809034,0.961129784,0.959414206,0.957662364,0.955874327,0.954050159,0.952189932,0.950293715,0.94836158,0.9463936,0.944389848,0.942350402,0.940275338,0.938164734,0.936018671,0.933837229,0.931620491,0.92936854,0.927081462,0.924759343,0.922402271,0.920010335,0.917583626,0.915122235,0.912626255,0.91009578,0.907530907,0.904931732,0.902298353,0.89963087,0.896929383,0.894193996,0.891424811,0.888621932,0.885785467,0.882915521,0.880012204,0.877075625,0.874105896,0.871103127,0.868067434,0.864998931,0.861897733,0.858763958,0.855597725,0.852399152,0.849168362,0.845905475,0.842610616,0.839283909,0.835925479,0.832535454,0.829113962,0.825661132,0.822177094,0.818661981,0.815115924,0.811539059,0.80793152,0.804293444,0.800624968,0.79692623,0.793197372,0.789438533,0.785649855,0.781831482,0.777983559,0.77410623,0.770199643,0.766263944,0.762299282,0.758305808,0.754283673,0.750233028,0.746154026,0.742046822,0.737911571,0.733748429,0.729557554,0.725339104,0.721093238,0.716820117,0.712519902,0.708192756,0.703838843,0.699458327,0.695051373,0.690618149,0.686158822,0.681673559,0.677162532,0.67262591,0.668063864,0.663476568,0.658864195,0.654226918,0.649564914,0.644878358,0.640167428,0.635432301,0.630673157,0.625890175,0.621083537,0.616253424,0.611400018,0.606523503,0.601624063,0.596701884,0.591757151,0.586790052,0.581800774,0.576789506,0.571756436,0.566701756,0.561625657,0.556528329,0.551409967,0.546270763,0.541110912,0.535930608,0.530730048,0.525509428,0.520268945,0.515008798,0.509729185,0.504430306,0.49911236,0.49377555,0.488420077,0.483046143,0.477653951,0.472243705,0.46681561,0.461369871,0.455906694,0.450426284,0.44492885,0.439414599,0.433883739,0.42833648,0.422773031,0.417193603,0.411598406,0.405987651,0.400361552,0.39472032,0.389064169,0.383393313,0.377707965,0.372008341,0.366294657,0.360567128,0.35482597,0.349071401,0.343303638,0.3375229,0.331729404,0.325923369,0.320105016,0.314274564,0.308432233,0.302578244,0.296712819,0.290836179,0.284948547,0.279050144,0.273141194,0.26722192,0.261292545,0.255353295,0.249404393,0.243446065,0.237478535,0.231502029,0.225516773,0.219522993,0.213520915,0.207510768,0.201492777,0.19546717,0.189434175,0.18339402,0.177346934,0.171293144,0.16523288,0.159166371,0.153093845,0.147015533,0.140931665,0.134842469,0.128748177,0.122649019,0.116545225,0.110437026,0.104324653,0.098208336,0.092088308,0.085964799,0.07983804,0.073708265,0.067575703,0.061440587,0.055303148,0.04916362,0.043022233,0.03687922,0.030734813,0.024589245,0.018442747,0.012295552,0.006147892,0};
  static uint8_t NR_init_counter = 0;
  uint8_t VAD_low = 0;
  uint8_t VAD_high = 127;
  float32_t lf_freq; // = (offset - width/2) / (12000 / NR_FFT_L); // bin BW is 46.9Hz [12000Hz / 256 bins] @96kHz
  float32_t uf_freq; //= (offset + width/2) / (12000 / NR_FFT_L);

  // TODO: calculate tinc from sample rate and decimation factor
#if 0 //FIXME
  const float32_t tinc = 0.00533333; // frame time 5.3333ms
  const float32_t tax = 0.0238;  // noise output smoothing time constant = -tinc/ln(0.8)
  const float32_t tap = 0.05062;  // speech prob smoothing time constant = -tinc/ln(0.9) tinc = frame time (5.33ms)
#else
  //compensated for 44.1khz, not 12khz...
//  const float32_t tinc = 0.00145; // frame time 5.3333ms
//  const float32_t tax = 0.0065;  // noise output smoothing time constant = -tinc/ln(0.8)
  const float32_t tax = -tinc / log(0.8);  // noise output smoothing time constant = -tinc/ln(0.8)
  const float32_t tap = -tinc / log(0.9);  // speech prob smoothing time constant = -tinc/ln(0.9) tinc = frame time (5.33ms)
#endif

  const float32_t psthr = 0.99; // threshold for smoothed speech probability [0.99]
  const float32_t pnsaf = 0.01; // noise probability safety value [0.01]
  const float32_t psini = 0.5; // initial speech probability [0.5]
  const float32_t pspri = 0.5; // prior speech probability [0.5]
  const float32_t tavini = 0.064;
  static float32_t ax; //=0.8;       // ax=exp(-tinc/tax); % noise output smoothing factor
  static float32_t ap; //=0.9;        // ap=exp(-tinc/tap); % noise output smoothing factor
  static float32_t xih1; // = 31.6;
  //xih1=10^(asnr/10); % speech-present SNR
  //static float32_t xih1r; //=-0.969346; // xih1r=1/(1+xih1)-1;
  ax = expf(-tinc / tax);
  ap = expf(-tinc / tap);
  xih1 = powf(10, (float32_t)asnr / 10.0);
  static float32_t xih1r = 1.0 / (1.0 + xih1) - 1.0;
  static float32_t pfac = (1.0 / pspri - 1.0) * (1.0 + xih1);
  float32_t snr_prio_min = powf(10, - (float32_t)20 / 20.0);
  //static float32_t pfac; //=32.6;  // pfac=(1/pspri-1)*(1+xih1); % p(noise)/p(speech)
  static float32_t pslp[NR_FFT_L / 2];
  static float32_t xt[NR_FFT_L / 2];
  static float32_t xtr;
  static float32_t pre_power;
  static float32_t post_power;
  static float32_t power_ratio;
  static int16_t NN;
  const int16_t NR_width = 4;
  const float32_t power_threshold = 0.4;
  float32_t ph1y[NR_FFT_L / 2];
  static int NR_first_time_2 = 1;

#if 1	//FIXME - Graham, check this is doing the right thing.
  //We don't have 'bands', as we are not a tuner. What we can do, later, is
  // have a setting for what 'mode' we are in (ssb, cw, am, fm even), and set the
  // cutoffs according to that.
  // guess at some lo/hi cutoff freqs - vaguely SSB
  lf_freq = 100;
  uf_freq = 3600; //2800 seemed OK
#else
  if (bands[current_band].FLoCut <= 0 && bands[current_band].FHiCut >= 0)
  {
    lf_freq = 0.0;
    uf_freq = fmax(-(float32_t)bands[current_band].FLoCut, (float32_t)bands[current_band].FHiCut);
  }
  else
  {
    if (bands[current_band].FLoCut > 0)
    {
      lf_freq = (float32_t)bands[current_band].FLoCut;
      uf_freq = (float32_t)bands[current_band].FHiCut;
    }
    else
    {
      uf_freq = -(float32_t)bands[current_band].FLoCut;
      lf_freq = -(float32_t)bands[current_band].FHiCut;
    }
  }
#endif

  // / rate DF SR[SAMPLE_RATE].rate/DF
  lf_freq /= ((SR[SAMPLE_RATE].rate / DF) / NR_FFT_L); // bin BW is 46.9Hz [12000Hz / 256 bins] @96kHz
  uf_freq /= ((SR[SAMPLE_RATE].rate / DF) / NR_FFT_L);

  // Frank DD4WH & Michael DL2FW, November 2017
  // NOISE REDUCTION BASED ON SPECTRAL SUBTRACTION
  // following Romanin et al. 2009 on the basis of Ephraim & Malah 1984 and Hu et al. 2001
  // detailed technical description of the implemented algorithm
  // can be found in our WIKI
  // https://github.com/df8oe/UHSDR/wiki/Noise-reduction
  //
  // half-overlapping input buffers (= overlap 50%)
  // sqrt von Hann window before FFT
  // sqrt von Hann window after inverse FFT
  // FFT256 - inverse FFT256
  // overlap-add

  // INITIALIZATION ONCE 1
  if (NR_first_time_2 == 1)
  { // TODO: properly initialize all the variables
    for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++)
    {
      NR_last_sample_buffer_L[bindx] = 0.0;
      NR_G[bindx] = 1.0;
      //xu[bindx] = 1.0;  //has to be replaced by other variable
      NR_Hk_old[bindx] = 1.0; // old gain or xu in development mode
      NR_Nest[bindx][0] = 0.0;
      NR_Nest[bindx][1] = 1.0;
      pslp[bindx] = 0.5;
    }
    NR_first_time_2 = 2; // we need to do some more a bit later down
  }



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
    // perform windowing on samples in the NR_FFT_buffer
    for (int idx = 0; idx < NR_FFT_L; idx++)
    { // sqrt Hann window
      //float32_t temp_sample = 0.5 * (float32_t)(1.0 - (cosf(PI * 2.0 * (float32_t)idx / (float32_t)((NR_FFT_L) - 1))));
      //NR_FFT_buffer[idx * 2] *= temp_sample;
      NR_FFT_buffer[idx * 2] *= sqrtHann[idx];
    }
#endif

    // NR_FFT
    // calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
    arm_cfft_f32(NR_FFT, NR_FFT_buffer, 0, 1);

    //##########################################################################################################################################
    //##########################################################################################################################################
    //##########################################################################################################################################

    for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++)
    {
      // this is squared magnitude for the current frame
      NR_X[bindx][0] = (NR_FFT_buffer[bindx * 2] * NR_FFT_buffer[bindx * 2] + NR_FFT_buffer[bindx * 2 + 1] * NR_FFT_buffer[bindx * 2 + 1]);
    }

    if (NR_first_time_2 == 2)
    { // TODO: properly initialize all the variables
      for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++)
      {
        NR_Nest[bindx][0] = NR_Nest[bindx][0] + 0.05 * NR_X[bindx][0]; // we do it 20 times to average over 20 frames for app. 100ms only on NR_on/bandswitch/modeswitch,...
        xt[bindx] = psini * NR_Nest[bindx][0];
      }
      NR_init_counter++;
      if (NR_init_counter > 19)//average over 20 frames for app. 100ms
      {
        NR_init_counter = 0;
        NR_first_time_2 = 3;  // now we did all the necessary initialization to actually start the noise reduction
      }
    }

    if (NR_first_time_2 == 3)
    {

      //new noise estimate MMSE based!!!

      for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // 1. Step of NR - calculate the SNR's
      {
        ph1y[bindx] = 1.0 / (1.0 + pfac * expf(xih1r * NR_X[bindx][0] / xt[bindx]));
        pslp[bindx] = ap * pslp[bindx] + (1.0 - ap) * ph1y[bindx];

        if (pslp[bindx] > psthr)
        {
          ph1y[bindx] = 1.0 - pnsaf;
        }
        else
        {
          ph1y[bindx] = fmin(ph1y[bindx] , 1.0);
        }
        xtr = (1.0 - ph1y[bindx]) * NR_X[bindx][0] + ph1y[bindx] * xt[bindx];
        xt[bindx] = ax * xt[bindx] + (1.0 - ax) * xtr;
      }



      for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // 1. Step of NR - calculate the SNR's
      {
        NR_SNR_post[bindx] = fmax(fmin(NR_X[bindx][0] / xt[bindx], 1000.0), snr_prio_min); // limited to +30 /-15 dB, might be still too much of reduction, let's try it?

        NR_SNR_prio[bindx] = fmax(NR_alpha * NR_Hk_old[bindx] + (1.0 - NR_alpha) * fmax(NR_SNR_post[bindx] - 1.0, 0.0), 0.0);
      }

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

      // 4    calculate v = SNRprio(n, bin[i]) / (SNRprio(n, bin[i]) + 1) * SNRpost(n, bin[i]) (eq. 12 of Schmitt et al. 2002, eq. 9 of Romanin et al. 2009)
      //      and calculate the HK's

      for (int bindx = VAD_low; bindx < VAD_high; bindx++) // maybe we should limit this to the signal containing bins (filtering!!)
      {
        float32_t v = NR_SNR_prio[bindx] * NR_SNR_post[bindx] / (1.0 + NR_SNR_prio[bindx]);

        NR_G[bindx] = 1.0 / NR_SNR_post[bindx] * sqrtf((0.7212 * v + v * v));

        NR_Hk_old[bindx] = NR_SNR_post[bindx] * NR_G[bindx] * NR_G[bindx]; //
      }

      // MUSICAL NOISE TREATMENT HERE, DL2FW

      // musical noise "artefact" reduction by dynamic averaging - depending on SNR ratio
      pre_power = 0.0;
      post_power = 0.0;
      for (int bindx = VAD_low; bindx < VAD_high; bindx++)
      {
        pre_power += NR_X[bindx][0];
        post_power += NR_G[bindx] * NR_G[bindx]  * NR_X[bindx][0];
      }

      power_ratio = post_power / pre_power;
      if (power_ratio > power_threshold)
      {
        power_ratio = 1.0;
        NN = 1;
      }
      else
      {
        NN = 1 + 2 * (int)(0.5 + NR_width * (1.0 - power_ratio / power_threshold));
      }

      for (int bindx = VAD_low + NN / 2; bindx < VAD_high - NN / 2; bindx++)
      {
        NR_Nest[bindx][0] = 0.0;
        for (int m = bindx - NN / 2; m <= bindx + NN / 2; m++)
        {
          NR_Nest[bindx][0] += NR_G[m];
        }
        NR_Nest[bindx][0] /= (float32_t)NN;
      }

      // and now the edges - only going NN steps forward and taking the average
      // lower edge
      for (int bindx = VAD_low; bindx < VAD_low + NN / 2; bindx++)
      {
        NR_Nest[bindx][0] = 0.0;
        for (int m = bindx; m < (bindx + NN); m++)
        {
          NR_Nest[bindx][0] += NR_G[m];
        }
        NR_Nest[bindx][0] /= (float32_t)NN;
      }

      // upper edge - only going NN steps backward and taking the average
      for (int bindx = VAD_high - NN; bindx < VAD_high; bindx++)
      {
        NR_Nest[bindx][0] = 0.0;
        for (int m = bindx; m > (bindx - NN); m--)
        {
          NR_Nest[bindx][0] += NR_G[m];
        }
        NR_Nest[bindx][0] /= (float32_t)NN;
      }

      // end of edge treatment

      for (int bindx = VAD_low + NN / 2; bindx < VAD_high - NN / 2; bindx++)
      {
        NR_G[bindx] = NR_Nest[bindx][0];
      }
      // end of musical noise reduction


    } //end of "if ts.nr_first_time == 3"


    //##########################################################################################################################################
    //##########################################################################################################################################
    //##########################################################################################################################################

#if 1
    // FINAL SPECTRAL WEIGHTING: Multiply current FFT results with NR_FFT_buffer for 128 bins with the 128 bin-specific gain factors G
    //              for(int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // try 128:
    for (int bindx = 0; bindx < NR_FFT_L / 2; bindx++) // try 128:
    {
      NR_FFT_buffer[bindx * 2] = NR_FFT_buffer [bindx * 2] * NR_G[bindx] * NR_long_tone_gain[bindx]; // real part
      NR_FFT_buffer[bindx * 2 + 1] = NR_FFT_buffer [bindx * 2 + 1] * NR_G[bindx] * NR_long_tone_gain[bindx]; // imag part
      NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 2] = NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 2] * NR_G[bindx] * NR_long_tone_gain[bindx]; // real part conjugate symmetric
      NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 1] = NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 1] * NR_G[bindx] * NR_long_tone_gain[bindx]; // imag part conjugate symmetric
    }

#endif
    /*****************************************************************
       NOISE REDUCTION CODE ENDS HERE
     *****************************************************************/
    // very interesting!
    // if I leave the FFT_buffer as is and just multiply the 19 bins below with 0.1, the audio
    // is distorted a little bit !
    // To me, this is an indicator of a problem with windowing . . .
    //

#if 0
    for (int bindx = 1; bindx < 20; bindx++)
      // bins 2 to 29 attenuated
      // set real values to 0.1 of their original value
    {
      NR_FFT_buffer[bindx * 2] *= 0.1;
      //      NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 2] *= 0.1; //NR_iFFT_buffer[idx] * 0.1;
      NR_FFT_buffer[bindx * 2 + 1] *= 0.1; //NR_iFFT_buffer[idx] * 0.1;
      //      NR_FFT_buffer[NR_FFT_L * 2 - bindx * 2 - 1] *= 0.1; //NR_iFFT_buffer[idx] * 0.1;
    }
#endif

    // NR_iFFT
    // perform iFFT (in-place)
    arm_cfft_f32(NR_iFFT, NR_FFT_buffer, 1, 1);

    // perform windowing on samples in the NR_FFT_buffer
    for (int idx = 0; idx < NR_FFT_L; idx++)
    { // sqrt Hann window
      NR_FFT_buffer[idx * 2] *= sqrtHann[idx];
    }

    // do the overlap & add
    for (int i = 0; i < NR_FFT_L / 2; i++)
    { // take real part of first half of current iFFT result and add to 2nd half of last iFFT_result
      //              NR_output_audio_buffer[i + k * (NR_FFT_L / 2)] = NR_FFT_buffer[i * 2] + NR_last_iFFT_result[i];
      float_buffer_L[i + k * (NR_FFT_L / 2)] = NR_FFT_buffer[i * 2] + NR_last_iFFT_result[i];
      float_buffer_R[i + k * (NR_FFT_L / 2)] = float_buffer_L[i + k * (NR_FFT_L / 2)];
      // FIXME: take out scaling !
      //            in_buffer[i + k * (NR_FFT_L / 2)] *= 0.3;
    }
    for (int i = 0; i < NR_FFT_L / 2; i++)
    {
      NR_last_iFFT_result[i] = NR_FFT_buffer[NR_FFT_L + i * 2];
    }
    // end of "for" loop which repeats the FFT_iFFT_chain two times !!!
  }

  /*      for(int i = 0; i < NR_FFT_L; i++)
        {
            float_buffer_L [i] = NR_output_audio_buffer[i];  // * 1.6; // * 9.0; // * 5.0;
            float_buffer_R [i] = float_buffer_L [i];
        } */
} // end of Romanin algorithm
