/**
  ******************************************************************************
  * @file    filter_noise_reduction.h
  * @author  Giuseppe Callipo - IK8YFW - ik8yfw@libero.it
  * @version V1.0.0
  * @date    22-04-2018
  * @brief   Noise Reduction routines
  *
  *
  * NOTE: This file is part of RadioDSP project.
  *       See main.c file for additional project informations.
  ******************************************************************************/

#ifndef FILTER_NOISE_REDUCTION_H_INCLUDED
#define FILTER_NOISE_REDUCTION_H_INCLUDED

/* define the max number of samples used on Average filter */
#define MAX_FILTER_BUF 24

// Pass in to the function - not internally referenced.
extern uint16_t fnr_level; //1-3
#define FNR_LEVEL_MIN 1
#define FNR_LEVEL_MAX 3

extern uint16_t fnra_level; //4-6
#define FNRA_LEVEL_MIN 4
#define FNRA_LEVEL_MAX 6

extern volatile float outwork_fn;
float  fnrFilter_n(float inwork, uint16_t n);
float  fnrFilter_n_Average(float inwork ,uint16_t n);



#endif /* FILTER_NOISE_REDUCTION_H_INCLUDED */

/**************************************END OF FILE****/
