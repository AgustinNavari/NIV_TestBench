/*
 * motor_commands.c
 *
 *  Created on: 2 ago 2026
 *      Author: agusn
 */

 #include "motor_commands.h"

 #include <errno.h>
 #include <limits.h>
 #include <stdio.h>
 #include <stdlib.h>
#include <inttypes.h>

 #include "esp_console.h"
 #include "esp_err.h"
 #include "esp_check.h"

 #include "motor_control.h"

 static int command_enable(int argc, char **argv)
 {
     (void)argc;
     (void)argv;

     esp_err_t error = motor_control_enable();

     if (error != ESP_OK)
     {
         printf("Error enabling motor: %s\n", esp_err_to_name(error));
         return 1;
     }

     printf("Motor enabled\n");

     return 0;
 }

 static int command_disable(int argc, char **argv)
 {
     (void)argc;
     (void)argv;

     esp_err_t error = motor_control_disable();

     if (error != ESP_OK)
     {
         printf("Error disabling motor: %s\n", esp_err_to_name(error));
         return 1;
     }

     printf("Motor disabled\n");

     return 0;
 }

 static int command_stop(int argc, char **argv)
 {
     (void)argc;
     (void)argv;

     esp_err_t error = motor_control_stop();

     if (error != ESP_OK)
     {
         printf("Error stopping motor: %s\n", esp_err_to_name(error));
         return 1;
     }

     printf("Motor stopped\n");

     return 0;
 }

 static int command_status(int argc, char **argv)
 {
     (void)argc;
     (void)argv;

     tic_status_t tic_status;

     esp_err_t err =
         motor_control_get_tic_status(&tic_status);

     if (err != ESP_OK)
     {
         printf(
             "Could not read Tic status: %s\n",
             esp_err_to_name(err)
         );

         return 1;
     }

     printf("\nTic status\n");
     printf("----------\n");

     printf(
         "Operation state: %u\n",
         tic_status.operation_state
     );

     printf(
         "Energized: %s\n",
         tic_status.energized ? "yes" : "no"
     );

     printf(
         "Position uncertain: %s\n",
         tic_status.position_uncertain ? "yes" : "no"
     );

     printf(
         "Forward limit: %s\n",
         tic_status.forward_limit_active ? "active" : "inactive"
     );

     printf(
         "Reverse limit: %s\n",
         tic_status.reverse_limit_active ? "active" : "inactive"
     );

     printf(
         "Homing: %s\n",
         tic_status.homing_active ? "active" : "inactive"
     );

     printf(
         "Error status: 0x%04X\n",
         tic_status.error_status
     );

     printf(
         "Current position: %" PRId32 "\n\n",
         tic_status.current_position
     );

     return 0;
 }

 static int command_move(int argc, char **argv)
 {
     if (argc != 2)
     {
         printf("Usage: move <target_position>\n");
         return 1;
     }

     errno = 0;

     char *end_pointer = NULL;
     long parsed_position = strtol(argv[1], &end_pointer, 10);

     if (errno != 0 ||
         end_pointer == argv[1] ||
         *end_pointer != '\0' ||
         parsed_position < INT32_MIN ||
         parsed_position > INT32_MAX)
     {
         printf("Invalid position: %s\n", argv[1]);
         return 1;
     }

     esp_err_t error =
         motor_control_move_to((int32_t)parsed_position);

     if (error != ESP_OK)
     {
         printf("Movement rejected: %s\n", esp_err_to_name(error));
         return 1;
     }

     printf("Target position accepted: %ld\n", parsed_position);

     return 0;
 }

 
 static int command_home(int argc, char **argv)
 {
     (void)argc;
     (void)argv;

     esp_err_t err = motor_control_home();

     if (err != ESP_OK)
     {
         printf(
             "Homing rejected: %s\n",
             esp_err_to_name(err)
         );

         return 1;
     }

     printf("Homing started\n");

     return 0;
 }
 
 esp_err_t motor_commands_register(void)
 {
     const esp_console_cmd_t enable_command = {
         .command = "enable",
         .help = "Enable the motor",
         .hint = NULL,
         .func = command_enable,
     };

     const esp_console_cmd_t disable_command = {
         .command = "disable",
         .help = "Disable the motor",
         .hint = NULL,
         .func = command_disable,
     };

     const esp_console_cmd_t stop_command = {
         .command = "stop",
         .help = "Stop the motor",
         .hint = NULL,
         .func = command_stop,
     };

     const esp_console_cmd_t status_command = {
         .command = "status",
         .help = "Show motor status",
         .hint = NULL,
         .func = command_status,
     };

     const esp_console_cmd_t move_command = {
         .command = "move",
         .help = "Move to an absolute target position",
         .hint = "<position>",
         .func = command_move,
     };
	 
	 const esp_console_cmd_t home_command = {
	     .command = "home",
	     .help = "Start the homing procedure",
	     .hint = NULL,
	     .func = command_home,
	 };
	 
	 ESP_RETURN_ON_ERROR(
	     esp_console_cmd_register(&home_command),
	     "MOTOR_COMMANDS",
	     "Could not register home command"
	 );

     ESP_RETURN_ON_ERROR(
         esp_console_cmd_register(&enable_command),
         "MOTOR_COMMANDS",
         "Could not register enable command"
     );

     ESP_RETURN_ON_ERROR(
         esp_console_cmd_register(&disable_command),
         "MOTOR_COMMANDS",
         "Could not register disable command"
     );

     ESP_RETURN_ON_ERROR(
         esp_console_cmd_register(&stop_command),
         "MOTOR_COMMANDS",
         "Could not register stop command"
     );

     ESP_RETURN_ON_ERROR(
         esp_console_cmd_register(&status_command),
         "MOTOR_COMMANDS",
         "Could not register status command"
     );

     ESP_RETURN_ON_ERROR(
         esp_console_cmd_register(&move_command),
         "MOTOR_COMMANDS",
         "Could not register move command"
     );

     return ESP_OK;
 }


