/*
 * scenario.c
 *
 *  Created on: 12 ago 2026
 *      Author: agusn
 */

 #include "scenario.h"

 #include <stddef.h>

 #include "esp_log.h"

 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"

 #include "motor_control.h"


 static const char *TAG = "SCENARIO";


 typedef struct
 {
     uint32_t time_ms;
     float volume_ml;
 } scenario_point_t;


 typedef struct
 {
     uint8_t id;
     const char *name;

     const scenario_point_t *points;
     size_t point_count;

     bool loop;

 } scenario_definition_t;


 /*
  * Scenario 1:
  *
  * 0 s -> 0 mL
  * 1 s -> 100 mL
  * 2 s -> 0 mL
  */
 static const scenario_point_t scenario_1_points[] =
 {
     {   0,   0.0f },
     {1000, 100.0f },
     {2000,   0.0f }
 };

 static const scenario_point_t scenario_2_points[] =
 {
     /* Inspiración */
     {   0,    0.0f },
     { 200,   25.0f },
     { 400,   90.0f },
     { 600,  190.0f },
     { 800,  310.0f },
     {1000,  410.0f },
     {1200,  475.0f },
     {1400,  500.0f },

     /* Espiración */
     {1600,  500.0f },
     {1800,  430.0f },
     {2000,  355.0f },
     {2200,  270.0f },
     {2400,  190.0f },
     {2600,  120.0f },
     {2800,   70.0f },
     {3000,   35.0f },
     {3200,   15.0f },
     {3400,    5.0f },
     {3600,    0.0f },
     {4000,    0.0f }
 };

 static const scenario_definition_t scenarios[] =
 {
     {
         .id = 1,
         .name = "Test 0-100-0 mL",
         .points = scenario_1_points,
         .point_count =
             sizeof(scenario_1_points) /
             sizeof(scenario_1_points[0]),
         .loop = false
     },

     {
         .id = 2,
         .name = "Physiological breathing",
         .points = scenario_2_points,
         .point_count =
             sizeof(scenario_2_points) /
             sizeof(scenario_2_points[0]),
         .loop = true
     }
 };


 static scenario_state_t scenario_state =
     SCENARIO_STATE_IDLE;

 static TaskHandle_t scenario_task_handle = NULL;

 static bool stop_requested = false;
 
 static const scenario_definition_t *scenario_find(uint8_t id)
 {
     const size_t scenario_count =
         sizeof(scenarios) / sizeof(scenarios[0]);

     for (size_t i = 0; i < scenario_count; i++)
     {
         if (scenarios[i].id == id)
         {
             return &scenarios[i];
         }
     }

     return NULL;
 }
 
 static void scenario_task(void *argument)
 {
     const scenario_definition_t *scenario =
         (const scenario_definition_t *)argument;

     ESP_LOGI(
         TAG,
         "Starting scenario %u: %s",
         scenario->id,
         scenario->name
     );

     scenario_state = SCENARIO_STATE_RUNNING;
     stop_requested = false;

     do
     {
         for (size_t i = 0; i < scenario->point_count - 1; i++)
         {
             if (stop_requested)
             {
                 break;
             }

             const scenario_point_t *point_a =
                 &scenario->points[i];

             const scenario_point_t *point_b =
                 &scenario->points[i + 1];

             uint32_t delta_time_ms =
                 point_b->time_ms - point_a->time_ms;

             float delta_volume_ml =
                 point_b->volume_ml - point_a->volume_ml;

             if (delta_time_ms == 0)
             {
                 ESP_LOGE(
                     TAG,
                     "Invalid scenario: two points have same time"
                 );

                 scenario_state = SCENARIO_STATE_ERROR;
                 break;
             }

             float delta_time_s =
                 (float)delta_time_ms / 1000.0f;

             float flow_ml_s =
                 delta_volume_ml / delta_time_s;


             esp_err_t err =
                 motor_control_set_flow(flow_ml_s);

             if (err != ESP_OK)
             {
                 ESP_LOGE(
                     TAG,
                     "Could not set flow: %s",
                     esp_err_to_name(err)
                 );

                 scenario_state = SCENARIO_STATE_ERROR;
                 break;
             }

             ulTaskNotifyTake(
                 pdTRUE,
                 pdMS_TO_TICKS(delta_time_ms)
             );
         }

         /*
          * Si hubo error, salimos aunque sea loop.
          */
         if (scenario_state == SCENARIO_STATE_ERROR)
         {
             break;
         }

     }
     while (scenario->loop && !stop_requested);


     motor_control_set_flow(0.0f);

     if (scenario_state != SCENARIO_STATE_ERROR)
     {
         if (stop_requested)
         {
             ESP_LOGI(TAG, "Scenario stopped");
             scenario_state = SCENARIO_STATE_IDLE;
         }
         else
         {
             ESP_LOGI(TAG, "Scenario finished");
             scenario_state = SCENARIO_STATE_FINISHED;
         }
     }

     scenario_task_handle = NULL;

     vTaskDelete(NULL);
 }
 
 esp_err_t scenario_init(void)
 {
     scenario_state = SCENARIO_STATE_IDLE;
     scenario_task_handle = NULL;
     stop_requested = false;

     ESP_LOGI(TAG, "Scenario module initialized");

     return ESP_OK;
 }
 
 esp_err_t scenario_start(uint8_t scenario_id)
 {
     if (scenario_state == SCENARIO_STATE_RUNNING)
     {
         ESP_LOGE(TAG, "A scenario is already running");
         return ESP_ERR_INVALID_STATE;
     }


     const scenario_definition_t *scenario =
         scenario_find(scenario_id);

     if (scenario == NULL)
     {
         ESP_LOGE(
             TAG,
             "Scenario %u does not exist",
             scenario_id
         );

         return ESP_ERR_NOT_FOUND;
     }


     stop_requested = false;


     BaseType_t result = xTaskCreate(
         scenario_task,
         "scenario_task",
         4096,
         (void *)scenario,
         5,
         &scenario_task_handle
     );


     if (result != pdPASS)
     {
         scenario_task_handle = NULL;

         ESP_LOGE(TAG, "Could not create scenario task");

         return ESP_ERR_NO_MEM;
     }


     return ESP_OK;
 }
 
 esp_err_t scenario_stop(void)
 {
     if (scenario_state != SCENARIO_STATE_RUNNING)
     {
         ESP_LOGW(TAG, "No scenario is running");
         return ESP_ERR_INVALID_STATE;
     }


     stop_requested = true;


     /*
      * Detenemos el motor inmediatamente.
      */
     esp_err_t err =
         motor_control_set_flow(0.0f);

     if (err != ESP_OK)
     {
         ESP_LOGE(
             TAG,
             "Could not stop motor: %s",
             esp_err_to_name(err)
         );
     }


     /*
      * Despertamos la scenario_task por si estaba
      * esperando el final de un segmento.
      */
     if (scenario_task_handle != NULL)
     {
         xTaskNotifyGive(scenario_task_handle);
     }


     return err;
 }
 
 scenario_state_t scenario_get_state(void)
 {
     return scenario_state;
 }


 bool scenario_is_running(void)
 {
     return scenario_state == SCENARIO_STATE_RUNNING;
 }