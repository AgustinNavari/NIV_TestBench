#ifndef SCENARIO_H
#define SCENARIO_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum
{
    SCENARIO_STATE_IDLE,
    SCENARIO_STATE_RUNNING,
    SCENARIO_STATE_FINISHED,
    SCENARIO_STATE_ERROR
} scenario_state_t;

esp_err_t scenario_init(void);

esp_err_t scenario_start(uint8_t scenario_id);

esp_err_t scenario_stop(void);

scenario_state_t scenario_get_state(void);

bool scenario_is_running(void);

#endif