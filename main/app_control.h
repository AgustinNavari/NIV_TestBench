/*
 * app_control.h
 *
 *  Created on: 13 ago 2026
 *      Author: agusn
 */

 #ifndef APP_CONTROL_H
 #define APP_CONTROL_H

 #include <stdbool.h>

 #include "esp_err.h"

 typedef enum
 {
     APP_STATE_HOMING,
     APP_STATE_READY,
     APP_STATE_SCENARIO,
     APP_STATE_SERIAL

 } app_state_t;


 esp_err_t app_control_init(void);

 app_state_t app_control_get_state(void);

 bool app_control_serial_control_enabled(void);

 #endif