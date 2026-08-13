/*
 * tic_driver.h
 *
 *  Created on: 2 ago 2026
 *      Author: agusn
 */

 #ifndef TIC_DRIVER_H
 #define TIC_DRIVER_H

 #include <stdbool.h>
 #include <stdint.h>

 #include "esp_err.h"

 typedef struct
 {
     uint8_t operation_state;

     bool energized;
     bool position_uncertain;
     bool forward_limit_active;
     bool reverse_limit_active;
     bool homing_active;

     uint16_t error_status;
     int32_t current_position;
 } tic_status_t;
 
 typedef enum
 {
     TIC_HOME_REVERSE = 0,
     TIC_HOME_FORWARD = 1
 } tic_home_direction_t;
 
 esp_err_t tic_driver_go_home(tic_home_direction_t direction);

 esp_err_t tic_driver_init(void);

 esp_err_t tic_driver_get_status(tic_status_t *status);

 esp_err_t tic_driver_energize(void);
 esp_err_t tic_driver_deenergize(void);
 esp_err_t tic_driver_set_target_position(int32_t target_position);
 esp_err_t tic_driver_halt_and_hold(void);
 
 esp_err_t tic_driver_exit_safe_start(void);
 
 esp_err_t tic_driver_reset_command_timeout(void);
 esp_err_t tic_driver_set_target_velocity(int32_t velocity);

 esp_err_t tic_driver_halt_and_set_position(int32_t position);
 #endif