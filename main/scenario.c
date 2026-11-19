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

     /*
      * Parámetros iniciales / normales.
      */
     float respiratory_rate_bpm;
     float tidal_volume_ml;

     /*
      * Suspiro.
      */
     bool sigh_enabled;
     uint8_t normal_breaths_before_sigh;
     float sigh_tidal_volume_ml;

     /*
      * Barrido progresivo.
      */
     bool sweep_enabled;
     float final_respiratory_rate_bpm;
     float final_tidal_volume_ml;
     uint32_t sweep_duration_s;

     /*
      * 0 = sin timeout automático.
      */
     uint32_t max_duration_s;

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
	    .sigh_tidal_volume_ml = 900.0f,
		
		.max_duration_s = 0
	},
	

     {
         .id = 2,
         .name = "Normal 500 mL",
         .respiratory_rate_bpm = 30.0f,
         .tidal_volume_ml = 300.0f,
		 .max_duration_s = 5 * 60
     },

     {
         .id = 3,
         .name = "Fast deep breathing",
         .respiratory_rate_bpm = 35.0f,
         .tidal_volume_ml = 650.0f,
		 
		 .max_duration_s = 5 * 60
     },

     {
         .id = 4,
         .name = "Relaxed breathing",
         .respiratory_rate_bpm = 15.0f,
         .tidal_volume_ml = 400.0f,
		 .max_duration_s = 5 * 60
     },
	 
	 {
	     .id = 5,
	     .name = "Patient evolution",

	     /*
	      * Comienza en:
	      * RR = 35 bpm
	      * VT = 600 mL
	      */
	     .respiratory_rate_bpm = 37.0f,
	     .tidal_volume_ml = 300.0f,

	     .sweep_enabled = true,

	     /*
	      * Termina en:
	      * RR = 20 bpm
	      * VT = 400 mL
	      */
	     .final_respiratory_rate_bpm = 20.0f,
	     .final_tidal_volume_ml = 700.0f,

	     /*
	      * 15 minutos.
	      */
	     .sweep_duration_s = 15 * 60,

	     /*
	      * No vuelve al basal automáticamente
	      * después de 5 minutos.
	      */
	     .max_duration_s = 0
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

     /*
      * Inicio del escenario actual.
      * Se usa para calcular el progreso del sweep.
      */
     int64_t scenario_start_us =
         esp_timer_get_time();

     /*
      * Inicio de la respiración actual.
      */
     int64_t breath_start_us =
         scenario_start_us;

     /*
      * RR y VT usados durante la respiración actual.
      * Permanecen constantes hasta terminar ese ciclo.
      */
     float current_rr =
         scenario->respiratory_rate_bpm;

     float current_vt =
         scenario->tidal_volume_ml;

     /*
      * Contador utilizado por el escenario con suspiros.
      */
     uint32_t breath_number = 0;

     TickType_t last_wake_time =
         xTaskGetTickCount();


     while (!stop_requested)
     {
         const int64_t now_us =
             esp_timer_get_time();


         /*
          * Duración de ESTA respiración.
          */
         const float breath_period_s =
             60.0f / current_rr;

         const int64_t breath_period_us =
             (int64_t)(
                 breath_period_s *
                 1000000.0f
             );


         /*
          * ¿Terminó la respiración actual?
          */
         if ((now_us - breath_start_us) >=
             breath_period_us)
         {
             /*
              * Usamos el límite teórico del ciclo
              * para no acumular el error del task de 10 ms.
              */
             const int64_t breath_boundary_us =
                 breath_start_us +
                 breath_period_us;


             /*
              * =====================================
              * CAMBIO DE ESCENARIO PENDIENTE
              * =====================================
              */
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
                      * exactamente entre respiraciones.
                      */
                     scenario_start_us =
                         breath_boundary_us;

                     breath_start_us =
                         breath_boundary_us;

                     breath_number = 0;

                     current_rr =
                         scenario->respiratory_rate_bpm;

                     current_vt =
                         scenario->tidal_volume_ml;
                 }
                 else
                 {
                     pending_scenario_id = 0;
                 }
             }

             /*
              * =====================================
              * CONTINUAMOS EL MISMO ESCENARIO
              * =====================================
              */
             else
             {
                 breath_start_us =
                     breath_boundary_us;

                 breath_number++;


                 /*
                  * =================================
                  * ESCENARIO CON BARRIDO
                  * =================================
                  */
                 if (scenario->sweep_enabled)
                 {
                     const float elapsed_s =
                         (float)(
                             breath_boundary_us -
                             scenario_start_us
                         ) /
                         1000000.0f;

                     float progress =
                         elapsed_s /
                         (float)
                         scenario->sweep_duration_s;

                     if (progress > 1.0f)
                     {
                         progress = 1.0f;
                     }


                     /*
                      * Interpolación lineal:
                      *
                      * RR:
                      * 35 -> 20 bpm
                      *
                      * VT:
                      * 600 -> 400 mL
                      */
                     current_rr =
                         scenario->respiratory_rate_bpm +
                         progress *
                         (
                             scenario->final_respiratory_rate_bpm -
                             scenario->respiratory_rate_bpm
                         );

                     current_vt =
                         scenario->tidal_volume_ml +
                         progress *
                         (
                             scenario->final_tidal_volume_ml -
                             scenario->tidal_volume_ml
                         );


                     ESP_LOGI(
                         TAG,
                         "Sweep: %.1f %%  RR=%.2f bpm  VT=%.1f mL",
                         progress * 100.0f,
                         current_rr,
                         current_vt
                     );
                 }

                 /*
                  * =================================
                  * ESCENARIO CON SUSPIROS
                  * =================================
                  */
                 else if (scenario->sigh_enabled)
                 {
                     const uint32_t cycle_length =
                         scenario->normal_breaths_before_sigh +
                         1;

                     const uint32_t breath_in_cycle =
                         breath_number %
                         cycle_length;


                     if (breath_in_cycle ==
                         scenario->normal_breaths_before_sigh)
                     {
                         current_vt =
                             scenario->sigh_tidal_volume_ml;
                     }
                     else
                     {
                         current_vt =
                             scenario->tidal_volume_ml;
                     }

                     current_rr =
                         scenario->respiratory_rate_bpm;
                 }

                 /*
                  * =================================
                  * ESCENARIO NORMAL
                  * =================================
                  */
                 else
                 {
                     current_rr =
                         scenario->respiratory_rate_bpm;

                     current_vt =
                         scenario->tidal_volume_ml;
                 }
             }
         }


         /*
          * Tiempo dentro de la respiración actual.
          */
         const float breath_time_s =
             (float)(
                 now_us -
                 breath_start_us
             ) /
             1000000.0f;


         /*
          * RR y VT permanecen constantes durante
          * toda la respiración actual.
          */
         const float target_volume_ml =
             breath_reference_get_volume(
                 current_rr,
                 current_vt,
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
 
 uint32_t scenario_get_max_duration_s(uint8_t scenario_id)
 {
     const scenario_definition_t *scenario =
         scenario_find(scenario_id);

     if (scenario == NULL)
     {
         return 0;
     }

     return scenario->max_duration_s;
 }
 

 bool scenario_is_running(void)
 {
     return scenario_state == SCENARIO_STATE_RUNNING;
 }
 
 bool scenario_change_pending(void)
 {
     return pending_scenario_id != 0;
 }
 
 uint8_t scenario_get_count(void)
 {
     return (uint8_t)(
         sizeof(scenarios) /
         sizeof(scenarios[0])
     );
 }