#include "motor_control.h"

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

#include "esp_log.h"

#include "tic_driver.h"

#define MOTOR_HOME_DIRECTION TIC_HOME_REVERSE

static const char *TAG = "MOTOR_CONTROL";

static bool motor_enabled = false;
static int32_t current_position = 0;
static int32_t target_position = 0;


esp_err_t motor_control_init(void)
{
    motor_enabled = false;
    current_position = 0;
    target_position = 0;

    esp_err_t err = tic_driver_init();

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Could not initialize Tic driver: %s",
            esp_err_to_name(err)
        );

        return err;
    }

    ESP_LOGI(TAG, "Motor control initialized");

    return ESP_OK;
}

esp_err_t motor_control_enable(void)
{
    esp_err_t err = tic_driver_energize();

    if (err != ESP_OK)
    {
        return err;
    }

    err = tic_driver_reset_command_timeout();

    if (err != ESP_OK)
    {
        tic_driver_deenergize();
        return err;
    }

    err = tic_driver_exit_safe_start();

    if (err != ESP_OK)
    {
        tic_driver_deenergize();
        return err;
    }

    motor_enabled = true;

    ESP_LOGI(TAG, "Motor enabled");

    return ESP_OK;
}

esp_err_t motor_control_disable(void)
{
    if (!motor_enabled)
    {
        ESP_LOGW(TAG, "Motor is already disabled");
        return ESP_OK;
    }

    esp_err_t err = tic_driver_deenergize();

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Could not de-energize Tic: %s",
            esp_err_to_name(err)
        );

        return err;
    }

    motor_enabled = false;

    ESP_LOGI(TAG, "Motor disabled");

    return ESP_OK;
}

esp_err_t motor_control_move_to(int32_t target)
{
    if (!motor_enabled)
    {
        ESP_LOGE(TAG, "Cannot move: motor disabled");
        return ESP_ERR_INVALID_STATE;
    }

    target_position = target;

    ESP_LOGI(
        TAG,
        "Moving from %" PRId32 " to %" PRId32 " [simulated]",
        current_position,
        target_position
    );

    /*
     * Simulación instantánea.
     * Más adelante el Tic ejecutará realmente el movimiento.
     */
    current_position = target_position;

    return ESP_OK;
}

esp_err_t motor_control_stop(void)
{
    target_position = current_position;

    ESP_LOGI(TAG, "Motor stopped");

    return ESP_OK;
}

bool motor_control_is_enabled(void)
{
    return motor_enabled;
}

int32_t motor_control_get_current_position(void)
{
    return current_position;
}

int32_t motor_control_get_target_position(void)
{
    return target_position;
}

esp_err_t motor_control_get_tic_status(tic_status_t *status)
{
    return tic_driver_get_status(status);
}

esp_err_t motor_control_home(void)
{
    if (!motor_enabled)
    {
        ESP_LOGE(TAG, "Cannot home: motor is disabled");
        return ESP_ERR_INVALID_STATE;
    }

    tic_status_t status;

    esp_err_t err = tic_driver_get_status(&status);

    if (err != ESP_OK)
    {
        return err;
    }

    if (status.homing_active)
    {
        ESP_LOGW(TAG, "Homing is already active");
        return ESP_ERR_INVALID_STATE;
    }

    err = tic_driver_reset_command_timeout();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not reset command timeout");
        return err;
    }

    err = tic_driver_exit_safe_start();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not exit safe start");
        return err;
    }

    err = tic_driver_go_home(MOTOR_HOME_DIRECTION);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start homing");
        return err;
    }

    ESP_LOGI(TAG, "Homing command accepted");

    return ESP_OK;
}