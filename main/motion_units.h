/*
 * motion_units.h
 *
 *  Created on: 12 ago 2026
 *      Author: agusn
 */

#include <stdint.h>

int32_t syringe_kinematics_ml_to_microsteps(float volume_ml);

float syringe_kinematics_microsteps_to_ml(int32_t microsteps);

int32_t syringe_kinematics_flow_to_tic_velocity(float flow_ml_s);

float syringe_kinematics_tic_velocity_to_flow(int32_t tic_velocity);




