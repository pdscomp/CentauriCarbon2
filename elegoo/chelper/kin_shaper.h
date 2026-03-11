/***************************************************************************** 
 * @Author       : Loping
 * @Date         : 2025-2-24 18:00:52
 * @LastEditors  : Loping
 * @LastEditTime : 2025-2-24 18:00:52
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#ifndef __KIN_SHAPER_H__
#define __KIN_SHAPER_H__

#include "itersolve.h" // struct stepper_kinematics

double input_shaper_get_step_generation_window(struct stepper_kinematics *sk);
struct stepper_kinematics * input_shaper_alloc(void);
int input_shaper_set_sk(
                struct stepper_kinematics *sk , struct stepper_kinematics *orig_sk);
int input_shaper_set_shaper_params(
                struct stepper_kinematics *sk, char axis , int n, double a[], double t[]);


#endif