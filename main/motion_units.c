/*
 * motion_units.c
 *
 *  Created on: 12 ago 2026
 *      Author: agusn
 */

 #include "motion_units.h"

 #include <math.h>


 #define SYRINGE_DIAMETER_MM        101.6f
 #define LEADSCREW_LEAD_MM_REV      8.0f
 #define MOTOR_FULL_STEPS_REV       200.0f
 #define MICROSTEPS_PER_FULL_STEP   4.0f


 #define TIC_VELOCITY_SCALE         10000.0f


 static float syringe_area_mm2(void)
 {
     const float radius_mm = SYRINGE_DIAMETER_MM / 2.0f;

     return (float)M_PI * radius_mm * radius_mm;
 }


 static float microsteps_per_mm(void)
 {
     const float microsteps_per_revolution =
         MOTOR_FULL_STEPS_REV * MICROSTEPS_PER_FULL_STEP;

     return microsteps_per_revolution /
            LEADSCREW_LEAD_MM_REV;
 }


 static float ml_per_mm(void)
 {

     return syringe_area_mm2() / 1000.0f;
 }


 int32_t syringe_kinematics_ml_to_microsteps(float volume_ml)
 {
     const float displacement_mm =
         volume_ml / ml_per_mm();

     const float microsteps =
         displacement_mm * microsteps_per_mm();

     return (int32_t)lroundf(microsteps);
 }


 float syringe_kinematics_microsteps_to_ml(int32_t microsteps)
 {
     const float displacement_mm =
         (float)microsteps / microsteps_per_mm();

     return displacement_mm * ml_per_mm();
 }


 int32_t syringe_kinematics_flow_to_tic_velocity(float flow_ml_s)
 {

     const float linear_velocity_mm_s =
         flow_ml_s / ml_per_mm();

     const float microsteps_per_second =
         linear_velocity_mm_s * microsteps_per_mm();

     const float tic_velocity =
         microsteps_per_second * TIC_VELOCITY_SCALE;

     return (int32_t)lroundf(tic_velocity);
 }


 float syringe_kinematics_tic_velocity_to_flow(int32_t tic_velocity)
 {
     const float microsteps_per_second =
         (float)tic_velocity / TIC_VELOCITY_SCALE;

     const float linear_velocity_mm_s =
         microsteps_per_second / microsteps_per_mm();

     return linear_velocity_mm_s * ml_per_mm();
 }