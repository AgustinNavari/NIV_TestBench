/*
 * app_control.c
 *
 *  Created on: 13 ago 2026
 *      Author: agusn
 */

 #include "app_control.h"

 #include <stdint.h>

 #include "esp_log.h"
 #include "esp_timer.h"

 #include "driver/gpio.h"

 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "esp_check.h"
 #include "motor_control.h"
 #include "scenario.h"
 
 #include <inttypes.h>
 
 static const char *TAG = "APP_CONTROL";

 /*
  *  GPIO del boton
  */
  
 #define BUTTON_GPIO GPIO_NUM_38
 #define BUTTON_POLL_MS        10
 #define BUTTON_DEBOUNCE_MS    50
 #define BUTTON_LONG_PRESS_MS  3000
 #define DEMANDING_TIMEOUT_US  (5LL * 60LL * 1000000LL)

 
 static app_state_t app_state =
     APP_STATE_HOMING;

 static uint8_t current_scenario = 0;

 static int64_t demanding_start_us = 0;


 /*
  * Button state
  */
 static bool button_previous_pressed = false;

 static int64_t button_press_start_us = 0;

 static bool long_press_triggered = false;
 
 static bool start_scenario_after_home = false;

 static void app_control_task(void *argument);
 static esp_err_t start_scenario_running(uint8_t id);
 
 static esp_err_t button_init(void)
 {
     gpio_config_t config = {
         .pin_bit_mask =
             1ULL << BUTTON_GPIO,

         .mode =
             GPIO_MODE_INPUT,

         .pull_up_en =
             GPIO_PULLUP_ENABLE,

         .pull_down_en =
             GPIO_PULLDOWN_DISABLE,

         .intr_type =
             GPIO_INTR_DISABLE
     };

     return gpio_config(&config);
 }

 
 esp_err_t app_control_init(void)
 {
     ESP_RETURN_ON_ERROR(
         button_init(),
         TAG,
         "Could not initialize button"
     );


     BaseType_t result =
         xTaskCreate(
             app_control_task,
             "app_control",
             4096,
             NULL,
             5,
             NULL
         );


     if (result != pdPASS)
     {
         return ESP_ERR_NO_MEM;
     }


     return ESP_OK;
 }
 

 static void stop_scenario_and_wait(void)
 {
     if (!scenario_is_running())
     {
         return;
     }

     scenario_stop();

     /*
      * Normalmente scenario_task sale en pocos ms.
      */
     for (int i = 0; i < 50; i++)
     {
         if (!scenario_is_running())
         {
             return;
         }

         vTaskDelay(pdMS_TO_TICKS(2));
     }

     ESP_LOGW(
         TAG,
         "Scenario did not stop in expected time"
     );
 }
 
 static esp_err_t start_homing(
     bool start_basal_after
 )
 {
     stop_scenario_and_wait();

     motor_control_stop();

     esp_err_t err =
         motor_control_enable();

     if (err != ESP_OK)
     {
         return err;
     }

     err = motor_control_home();

     if (err != ESP_OK)
     {
         return err;
     }

     start_scenario_after_home =
         start_basal_after;

     current_scenario = 0;

     app_state =
         APP_STATE_HOMING;

     return ESP_OK;
 }
 
 static void process_homing(void)
 {
     if (app_state != APP_STATE_HOMING)
     {
         return;
     }

     tic_status_t status;

     if (motor_control_get_tic_status(
             &status) != ESP_OK)
     {
         return;
     }

     if (status.homing_active ||
         status.position_uncertain)
     {
         return;
     }


     ESP_LOGI(
         TAG,
         "Homing complete"
     );


     if (start_scenario_after_home)
     {
         start_scenario_after_home = false;

         start_scenario_running(1);

         return;
     }


     /*
      * Home solicitado para volver a READY.
      */
     motor_control_disable();

     current_scenario = 0;

     app_state =
         APP_STATE_READY;

     ESP_LOGI(
         TAG,
         "READY - motor disabled"
     );
 }
 
 static esp_err_t start_scenario_running(uint8_t id)
 {
     stop_scenario_and_wait();

     esp_err_t err =
         scenario_start(id);

     if (err != ESP_OK)
     {
         ESP_LOGE(
             TAG,
             "Could not start scenario %u: %s",
             id,
             esp_err_to_name(err)
         );

         return err;
     }

     current_scenario = id;

     app_state =
         APP_STATE_SCENARIO;


     if (id > 1)
     {
         demanding_start_us =
             esp_timer_get_time();
     }
     else
     {
         demanding_start_us = 0;
     }


     ESP_LOGI(
         TAG,
         "Automatic scenario %u started",
         id
     );

     return ESP_OK;
 }
 
 static void handle_short_press(void)
 {
     switch (app_state)
     {
         case APP_STATE_READY:

             /*
              * Primer click:
              * referenciamos nuevamente
              * y arrancamos basal.
              */
             start_homing(true);

             break;


         case APP_STATE_SCENARIO:
         {
             uint8_t next =
                 current_scenario + 1;

             uint8_t count =
                 scenario_get_count();

             if (next > count)
             {
                 next = 1;
             }

             start_scenario_running(next);

             break;
         }


         case APP_STATE_SERIAL:

             /*
              * En modo serial ignoramos
              * clicks cortos.
              */
             break;


         case APP_STATE_HOMING:

             /*
              * No hacemos nada durante home.
              */
             break;
     }
 }
 
 static void handle_long_press(void)
 {
     if (app_state == APP_STATE_READY)
     {
         /*
          * Entrar a control manual.
          *
          * El motor empieza deshabilitado.
          * El usuario podrá ejecutar:
          *
          * enable
          * home
          * move...
          */
         app_state =
             APP_STATE_SERIAL;

         ESP_LOGI(
             TAG,
             "SERIAL CONTROL MODE"
         );

         return;
     }


     if (app_state == APP_STATE_HOMING)
     {
         return;
     }


     /*
      * Scenario o serial:
      * volvemos a cero y READY.
      */
     start_homing(false);
 }
 
 static void process_button(void)
 {
     const bool pressed =
         gpio_get_level(BUTTON_GPIO) == 0;


     /*
      * Flanco de bajada:
      * acaba de presionarse.
      */
     if (pressed &&
         !button_previous_pressed)
     {
         button_press_start_us =
             esp_timer_get_time();

         button_previous_pressed =
             true;

         long_press_triggered =
             false;

         return;
     }


     /*
      * Mientras sigue presionado,
      * revisar long press.
      */
     if (pressed &&
         button_previous_pressed &&
         !long_press_triggered)
     {
         int64_t elapsed_ms =
             (esp_timer_get_time() -
              button_press_start_us)
             / 1000;

         if (elapsed_ms >=
             BUTTON_LONG_PRESS_MS)
         {
             long_press_triggered =
                 true;

             handle_long_press();
         }

         return;
     }


     /*
      * Se acaba de soltar.
      */
     if (!pressed &&
         button_previous_pressed)
     {
         int64_t elapsed_ms =
             (esp_timer_get_time() -
              button_press_start_us)
             / 1000;

         button_previous_pressed =
             false;


         /*
          * Si ya disparamos long press,
          * soltarlo no genera short press.
          */
         if (long_press_triggered)
         {
             return;
         }


         if (elapsed_ms >=
             BUTTON_DEBOUNCE_MS)
         {
             handle_short_press();
         }
     }
 }
 
 static void process_scenario_timeout(void)
 {
     if (app_state !=
         APP_STATE_SCENARIO)
     {
         return;
     }

     /*
      * Scenario 1 no tiene timeout.
      */
     if (current_scenario <= 1)
     {
         return;
     }


     const int64_t elapsed_us =
         esp_timer_get_time() -
         demanding_start_us;


     if (elapsed_us >=
         DEMANDING_TIMEOUT_US)
     {
         ESP_LOGI(
             TAG,
             "Demanding scenario timeout -> basal"
         );

         start_scenario_running(1);
     }
 }
 
 static void app_control_task(
     void *argument
 )
 {
     /*
      * POWER ON:
      * hacemos home automáticamente.
      */
     ESP_ERROR_CHECK(
         start_homing(false)
     );


     TickType_t last_wake =
         xTaskGetTickCount();


     while (true)
     {
         process_button();

         process_homing();

         process_scenario_timeout();


         xTaskDelayUntil(
             &last_wake,
             pdMS_TO_TICKS(
                 BUTTON_POLL_MS
             )
         );
     }
 }
 
 app_state_t app_control_get_state(void)
 {
     return app_state;
 }


 bool app_control_serial_control_enabled(void)
 {
     return app_state ==
            APP_STATE_SERIAL;
 }