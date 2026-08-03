/*
 * motor_control.h
 *
 *  Created on: 2 ago 2026
 *      Author: agusn
 */

 #ifndef MOTOR_CONTROL_H
 #define MOTOR_CONTROL_H

 #include <stdbool.h>
 #include <stdint.h>

 #include "esp_err.h"
 #include "tic_driver.h"

 esp_err_t motor_control_init(void);

 esp_err_t motor_control_enable(void);
 esp_err_t motor_control_disable(void);

 esp_err_t motor_control_move_to(int32_t target_position);
 esp_err_t motor_control_move_relative(int32_t displacement);
 esp_err_t motor_control_stop(void);

 bool motor_control_is_enabled(void);
 int32_t motor_control_get_current_position(void);
 int32_t motor_control_get_target_position(void);
 
 esp_err_t motor_control_get_tic_status(tic_status_t *status);
 
 esp_err_t motor_control_home(void);

 #endif