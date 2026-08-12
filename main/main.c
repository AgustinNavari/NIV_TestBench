#include "esp_err.h"
#include "esp_log.h"

#include "console_app.h"
#include "motor_control.h"
#include "scenario.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting syringe controller");

    ESP_ERROR_CHECK(motor_control_init());

    ESP_ERROR_CHECK(scenario_init());

    ESP_ERROR_CHECK(console_app_init());

    ESP_LOGI(TAG, "Initialization complete");
}

