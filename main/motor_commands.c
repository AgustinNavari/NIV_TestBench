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
 #include "scenario.h"

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
 
 static int command_velocity(int argc, char **argv)
 {
     if (argc != 2)
     {
         printf("Usage: velocity <value>\n");
         return 1;
     }

     errno = 0;

     char *end_pointer = NULL;
     long parsed_velocity = strtol(argv[1], &end_pointer, 10);

     if (errno != 0 ||
         end_pointer == argv[1] ||
         *end_pointer != '\0' ||
         parsed_velocity < INT32_MIN ||
         parsed_velocity > INT32_MAX)
     {
         printf("Invalid velocity: %s\n", argv[1]);
         return 1;
     }

     esp_err_t err = motor_control_set_velocity(
         (int32_t)parsed_velocity
     );

     if (err != ESP_OK)
     {
         printf(
             "Velocity command rejected: %s\n",
             esp_err_to_name(err)
         );

         return 1;
     }

     printf(
         "Target velocity: %ld\n",
         parsed_velocity
     );

     return 0;
 }
 
 static int command_flow(int argc, char **argv)
 {
     if (argc != 2)
     {
         printf("Usage: flow <ml_per_s>\n");
         return 1;
     }

     errno = 0;

     char *end_pointer = NULL;
     float flow_ml_s = strtof(argv[1], &end_pointer);

     if (errno != 0 ||
         end_pointer == argv[1] ||
         *end_pointer != '\0')
     {
         printf("Invalid flow: %s\n", argv[1]);
         return 1;
     }

     esp_err_t err =
         motor_control_set_flow(flow_ml_s);

     if (err != ESP_OK)
     {
         printf(
             "Flow command rejected: %s\n",
             esp_err_to_name(err)
         );

         return 1;
     }

     printf("Target flow: %.2f mL/s\n", flow_ml_s);

     return 0;
 }
 
 static int command_move_ml(int argc, char **argv)
 {
     if (argc != 2)
     {
         printf("Usage: move_ml <volume_ml>\n");
         return 1;
     }

     errno = 0;

     char *end_pointer = NULL;
     float volume_ml = strtof(argv[1], &end_pointer);

     if (errno != 0 ||
         end_pointer == argv[1] ||
         *end_pointer != '\0')
     {
         printf("Invalid volume: %s\n", argv[1]);
         return 1;
     }

     esp_err_t err =
         motor_control_move_to_volume(volume_ml);

     if (err != ESP_OK)
     {
         printf(
             "Move command rejected: %s\n",
             esp_err_to_name(err)
         );

         return 1;
     }

     printf("Target volume: %.2f mL\n", volume_ml);

     return 0;
 }
 
 static int command_scenario(int argc, char **argv)
 {
     if (argc != 2)
     {
         printf(
             "Usage:\n"
             "  scenario <id>\n"
             "  scenario stop\n"
             "  scenario status\n"
         );

         return 1;
     }


     /*
      * scenario stop
      */
     if (strcmp(argv[1], "stop") == 0)
     {
         esp_err_t err = scenario_stop();

         if (err != ESP_OK)
         {
             printf(
                 "Could not stop scenario: %s\n",
                 esp_err_to_name(err)
             );

             return 1;
         }

         printf("Scenario stop requested\n");

         return 0;
     }


     /*
      * scenario status
      */
     if (strcmp(argv[1], "status") == 0)
     {
         scenario_state_t state =
             scenario_get_state();

         switch (state)
         {
             case SCENARIO_STATE_IDLE:
                 printf("Scenario state: IDLE\n");
                 break;

             case SCENARIO_STATE_RUNNING:
                 printf("Scenario state: RUNNING\n");
                 break;

             case SCENARIO_STATE_ERROR:
                 printf("Scenario state: ERROR\n");
                 break;

             default:
                 printf("Scenario state: UNKNOWN\n");
                 break;
         }

         return 0;
     }


     /*
      * Si no fue "stop" ni "status",
      * interpretamos el argumento como ID.
      */

     errno = 0;

     char *end_pointer = NULL;

     long scenario_id =
         strtol(argv[1], &end_pointer, 10);


     if (errno != 0 ||
         end_pointer == argv[1] ||
         *end_pointer != '\0' ||
         scenario_id < 0 ||
         scenario_id > UINT8_MAX)
     {
         printf(
             "Invalid scenario ID: %s\n",
             argv[1]
         );

         return 1;
     }


     esp_err_t err =
         scenario_start((uint8_t)scenario_id);


     if (err != ESP_OK)
     {
         printf(
             "Could not start scenario: %s\n",
             esp_err_to_name(err)
         );

         return 1;
     }


     printf(
         "Scenario %ld started\n",
         scenario_id
     );

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
	 
	 const esp_console_cmd_t velocity_command = {
	     .command = "velocity",
	     .help = "Set Tic target velocity",
	     .hint = "<velocity>",
	     .func = command_velocity,
	 };
	 
	 const esp_console_cmd_t move_ml_command = {
	     .command = "move_ml",
	     .help = "Move syringe to a target volume",
	     .hint = "<volume_ml>",
	     .func = command_move_ml,
	 };
	 
	 const esp_console_cmd_t flow_command = {
	     .command = "flow",
	     .help = "Set syringe flow in mL/s",
	     .hint = "<ml_per_s>",
	     .func = command_flow,
	 };
	 
	 const esp_console_cmd_t scenario_command = {
	     .command = "scenario",
	     .help = "Run or control a predefined syringe scenario",
	     .hint = "<id|stop|status>",
	     .func = command_scenario,
	 };
	 
	 ESP_RETURN_ON_ERROR(
	     esp_console_cmd_register(&scenario_command),
	     "MOTOR_COMMANDS",
	     "Could not register scenario command"
	 );

	 ESP_RETURN_ON_ERROR(
	     esp_console_cmd_register(&flow_command),
	     "MOTOR_COMMANDS",
	     "Could not register flow command"
	 );

	 ESP_RETURN_ON_ERROR(
	     esp_console_cmd_register(&move_ml_command),
	     "MOTOR_COMMANDS", 
	     "Could not register move_ml command"
	 );
	 
	 ESP_RETURN_ON_ERROR(
	     esp_console_cmd_register(&velocity_command),
	     "MOTOR_COMMANDS",
	     "Could not register velocity command"
	 );
	 
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


