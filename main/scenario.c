/*
 * scenario.c
 *
 *  Created on: 12 ago 2026
 *      Author: agusn
 */

 #include "scenario.h"

 #include <stddef.h>

 #include "esp_log.h"
 #include "esp_timer.h"

 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"

 #include "motor_control.h"
 #include "breath_reference.h"
 
#define SCENARIO_UPDATE_MS 10


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

     float respiratory_rate_bpm;
     float tidal_volume_ml;

     bool sigh_enabled;
     uint8_t normal_breaths_before_sigh;
     float sigh_tidal_volume_ml;

 } scenario_definition_t;

 static const scenario_definition_t scenarios[] =
 {
	{
	    .id = 1,
	    .name = "Normal breathing with sigh",
	    .respiratory_rate_bpm = 22.0f,
	    .tidal_volume_ml = 500.0f,

	    .sigh_enabled = true,
	    .normal_breaths_before_sigh = 10,
	    .sigh_tidal_volume_ml = 900.0f
	},
	

     {
         .id = 2,
         .name = "Normal 500 mL",
         .respiratory_rate_bpm = 30.0f,
         .tidal_volume_ml = 300.0f
     },

     {
         .id = 3,
         .name = "Fast deep breathing",
         .respiratory_rate_bpm = 35.0f,
         .tidal_volume_ml = 650.0f
     },

     {
         .id = 4,
         .name = "Fast breathing",
         .respiratory_rate_bpm = 15.0f,
         .tidal_volume_ml = 300.0f
     }
 };


 static scenario_state_t scenario_state =
     SCENARIO_STATE_IDLE;

 static TaskHandle_t scenario_task_handle = NULL;

 static bool stop_requested = false;
 static volatile uint8_t pending_scenario_id = 0;
 
 static const scenario_definition_t *
 scenario_find(uint8_t id)
 {
     const size_t scenario_count =
         sizeof(scenarios) /
         sizeof(scenarios[0]);

     for (size_t i = 0;
          i < scenario_count;
          i++)
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

     scenario_state = SCENARIO_STATE_RUNNING;
     stop_requested = false;
     pending_scenario_id = 0;

     int64_t start_time_us =
         esp_timer_get_time();

     uint32_t previous_breath_index = 0;

     TickType_t last_wake_time =
         xTaskGetTickCount();

     while (!stop_requested)
     {
         const int64_t now_us =
             esp_timer_get_time();

         float time_s =
             (float)(now_us - start_time_us) /
             1000000.0f;

         /*
          * Determinamos en qué respiración del
          * escenario actual estamos.
          */
         const float breath_period_s =
             60.0f /
             scenario->respiratory_rate_bpm;

         uint32_t breath_index =
             (uint32_t)(
                 time_s /
                 breath_period_s
             );

         /*
          * Si cambió el índice significa que terminó
          * una respiración y comenzó la siguiente.
          *
          * Este es el único momento en el que
          * permitimos cambiar de escenario.
          */
         if (breath_index != previous_breath_index)
         {
             previous_breath_index =
                 breath_index;

             if (pending_scenario_id != 0)
             {
                 const scenario_definition_t *next_scenario =
                     scenario_find(
                         pending_scenario_id
                     );

                 if (next_scenario != NULL)
                 {
                     ESP_LOGI(
                         TAG,
                         "Scenario transition %u -> %u",
                         scenario->id,
                         next_scenario->id
                     );

                     scenario =
                         next_scenario;

                     pending_scenario_id = 0;

                     /*
                      * El nuevo escenario comienza
                      * exactamente en t = 0.
                      */
                     start_time_us =
                         now_us;

                     time_s = 0.0f;

                     previous_breath_index = 0;
                 }
                 else
                 {
                     pending_scenario_id = 0;
                 }
             }
         }


         /*
          * Generación de volumen objetivo.
          */

         float target_tidal_volume_ml =
             scenario->tidal_volume_ml;

         float breath_time_s =
             time_s;


         /*
          * Escenario con suspiros.
          */
         if (scenario->sigh_enabled)
         {
             const float current_breath_period_s =
                 60.0f /
                 scenario->respiratory_rate_bpm;

             const uint32_t current_breath_index =
                 (uint32_t)(
                     time_s /
                     current_breath_period_s
                 );

             const uint32_t cycle_length =
                 scenario->normal_breaths_before_sigh + 1;

             const uint32_t breath_in_cycle =
                 current_breath_index %
                 cycle_length;

             if (breath_in_cycle ==
                 scenario->normal_breaths_before_sigh)
             {
                 target_tidal_volume_ml =
                     scenario->sigh_tidal_volume_ml;
             }

             breath_time_s =
                 time_s -
                 ((float)current_breath_index *
                  current_breath_period_s);
         }


         const float target_volume_ml =
             breath_reference_get_volume(
                 scenario->respiratory_rate_bpm,
                 target_tidal_volume_ml,
                 breath_time_s
             );


         esp_err_t err =
             motor_control_move_to_volume(
                 target_volume_ml
             );

         if (err != ESP_OK)
         {
             ESP_LOGE(
                 TAG,
                 "Could not set target volume: %s",
                 esp_err_to_name(err)
             );

             scenario_state =
                 SCENARIO_STATE_ERROR;

             break;
         }


         vTaskDelayUntil(
             &last_wake_time,
             pdMS_TO_TICKS(
                 SCENARIO_UPDATE_MS
             )
         );
     }


     pending_scenario_id = 0;

     motor_control_stop();


     if (scenario_state !=
         SCENARIO_STATE_ERROR)
     {
         scenario_state =
             SCENARIO_STATE_IDLE;
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
     if (scenario_state ==
         SCENARIO_STATE_RUNNING)
     {
         ESP_LOGE(
             TAG,
             "A scenario is already running"
         );

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


     BaseType_t result =
         xTaskCreate(
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

         ESP_LOGE(
             TAG,
             "Could not create scenario task"
         );

         return ESP_ERR_NO_MEM;
     }


     return ESP_OK;
 }
 
 esp_err_t scenario_stop(void)
 {
     if (scenario_state !=
         SCENARIO_STATE_RUNNING)
     {
         ESP_LOGW(
             TAG,
             "No scenario is running"
         );

         return ESP_ERR_INVALID_STATE;
     }


     stop_requested = true;


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


     return err;
 }
 
 scenario_state_t scenario_get_state(void)
 {
     return scenario_state;
 }

 esp_err_t scenario_request_change(uint8_t scenario_id)
 {
     if (scenario_state != SCENARIO_STATE_RUNNING)
     {
         return ESP_ERR_INVALID_STATE;
     }

     const scenario_definition_t *scenario =
         scenario_find(scenario_id);

     if (scenario == NULL)
     {
         return ESP_ERR_NOT_FOUND;
     }

     pending_scenario_id = scenario_id;

     ESP_LOGI(
         TAG,
         "Scenario %u requested - waiting for end of breath",
         scenario_id
     );

     return ESP_OK;
 }
 

 bool scenario_is_running(void)
 {
     return scenario_state == SCENARIO_STATE_RUNNING;
 }
 
 uint8_t scenario_get_count(void)
 {
     return (uint8_t)(
         sizeof(scenarios) /
         sizeof(scenarios[0])
     );
 }