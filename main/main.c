#include "esp_err.h"
#include "esp_log.h"

#include "console_app.h"
#include "motor_control.h"
#include "scenario.h"
#include "app_control.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(
        TAG,
        "Starting syringe controller"
    );

    ESP_ERROR_CHECK(
        motor_control_init()
    );

    ESP_ERROR_CHECK(
        scenario_init()
    );

    /*
     * La consola sigue existiendo siempre.
     */
    ESP_ERROR_CHECK(
        console_app_init()
    );

    /*
     * Control automático + botón.
     */
    ESP_ERROR_CHECK(
        app_control_init()
    );

    ESP_LOGI(
        TAG,
        "Initialization complete"
    );
}
