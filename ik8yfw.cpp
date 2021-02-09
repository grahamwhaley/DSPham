/**
  ******************************************************************************
  * @file    main.c
  * @author  Giuseppe Callipo - IK8YFW - ik8yfw@libero.it
  * @version V1.0.0
  * @date    22-04-2018
  * @brief   IO Routine
  *
  *
  * NOTE: This file is part of RadioDSP project.
  *       See main.c file for additional project informations.
  *       
  *       Code taken from github.com/gcallipo/RadioDSP-Stm32f103/src/RadioDSP_Fast/src
  *       License was CC
  ******************************************************************************/
#include <Audio.h>
#include <arm_math.h>
#include <arm_const_structs.h>

#include "global.h"

#include "ik8yfw.h"

// Pass these in to the functions.
// Simply global here to keep the code together.

uint16_t fnr_level = 2; //1-3
uint16_t fnra_level = 5; //4-6

volatile float outwork_fn  = 0;
static float buffer[MAX_FILTER_BUF];

// Variable noise filter (Exponential Smooting Moving Filter) : n: 3 (max) - 1 (min)
float fnrFilter_n(float inwork, uint16_t n)
{

	float coeff;
	if (n==1) coeff = 0.5;
	if (n==2) coeff = 0.3;
	if (n==3) coeff = 0.1;

    outwork_fn += (inwork - outwork_fn) * coeff;
    return outwork_fn;
 }

// Variable noise filter (Average Smooting Moving Filter) : n: 6 (max) - 4 (min)
float fnrFilter_n_Average(float inwork ,uint16_t n)
{
  int idx_max_buf;
  if (n==4) idx_max_buf = 8;
	if (n==5) idx_max_buf = 16;
	if (n==6) idx_max_buf = 24;

	int i;
  float out = 0;

  for (i = (idx_max_buf-1); i > 0; i--)
	{
		buffer[i] = buffer[i-1];
		out+=buffer[i];
	}
	buffer[0] = inwork;
	out+=inwork;
	out=(out/idx_max_buf)*0.8;

  return out;
}
