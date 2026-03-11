#ifndef KIN_EXTRUDER_H
#define KIN_EXTRUDER_H

#include "itersolve.h" // struct stepper_kinematics

struct stepper_kinematics *extruder_stepper_alloc(void);

void extruder_stepper_free(struct stepper_kinematics *sk);

void extruder_set_pressure_advance(
    struct stepper_kinematics *sk, double print_time
    , double pressure_advance, double smooth_time);
#endif // kin_extruder.h