extern void nr_kim();
extern void nr_kim_init();

//Noise reduction noise floor?
extern float32_t NR_PSI; // default 3.0, range of 2.5 - 3.5 ?; 6.0 leads to strong reverb effects
#define KIM_NR_PSI_MIN 1.0
#define KIM_NR_PSI_MAX 6.0

extern float32_t NR_KIM_K;  // 0.8 - 1.0 // 'strength' of reduction
#define KIM_NR_KIM_K_MIN 0.8
#define KIM_NR_KIM_K_MAX 1.0

extern float32_t NR_alpha;
#define KIM_NR_ALPHA_MIN 0.7
#define KIM_NR_ALPHA_MAX 0.99
extern float32_t NR_onemalpha;  //NR_onemalpha = (1.0 - NR_alpha);

extern float32_t NR_beta;
#define KIM_NR_BETA_MIN 0.1
#define KIM_NR_BETA_MAX 0.99
extern float32_t NR_onemtwobeta;  //NR_onemtwobeta = (1.0 - (2.0 * NR_beta));
