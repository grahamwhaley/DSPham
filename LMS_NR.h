
void Init_LMS_NR ();
extern void LMS_NoiseReduction(int16_t blockSize, float32_t *nrbuffer);

extern int LMS_nr_strength; //Range 0-40  (default is 5)
#define LMS_MIN_STRENGTH 0
#define LMS_MAX_STRENGTH 40
