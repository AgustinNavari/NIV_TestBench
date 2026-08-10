/*
 * tic_driver.c
 *
 *  Created on: 2 ago 2026
 *      Author: agusn
 */

 #include "tic_driver.h"

 #include <inttypes.h>

 #include "driver/i2c_master.h"
 #include "esp_check.h"
 #include "esp_log.h"

 static const char *TAG = "TIC_DRIVER";

 #define TIC_I2C_SDA_GPIO       GPIO_NUM_11
 #define TIC_I2C_SCL_GPIO       GPIO_NUM_12
 #define TIC_I2C_FREQUENCY_HZ   100000
 #define TIC_I2C_ADDRESS        0x0E
 
 #define TIC_CMD_GET_VARIABLE       0xA1

 #define TIC_VAR_OPERATION_STATE    0x00
 #define TIC_VAR_MISC_FLAGS         0x01
 #define TIC_VAR_ERROR_STATUS       0x02
 #define TIC_VAR_CURRENT_POSITION   0x22

 #define TIC_I2C_TIMEOUT_MS         100
 
 #define TIC_CMD_EXIT_SAFE_START  0x83
 #define TIC_CMD_ENERGIZE         0x85
 #define TIC_CMD_DEENERGIZE       0x86
 
 #define TIC_CMD_GO_HOME 0x97
 
 #define TIC_CMD_SET_TARGET_POSITION  0xE0
 
 #define TIC_CMD_RESET_COMMAND_TIMEOUT 0x8C

 static i2c_master_bus_handle_t i2c_bus_handle = NULL;
 static i2c_master_dev_handle_t tic_device_handle = NULL;

 
 
 
 esp_err_t tic_driver_init(void)
 {
     const i2c_master_bus_config_t bus_config = {
         .i2c_port = I2C_NUM_0,
         .sda_io_num = TIC_I2C_SDA_GPIO,
         .scl_io_num = TIC_I2C_SCL_GPIO,
         .clk_source = I2C_CLK_SRC_DEFAULT,
         .glitch_ignore_cnt = 7,
         .intr_priority = 0,
         .trans_queue_depth = 0,
         .flags.enable_internal_pullup = true,
     };

     ESP_RETURN_ON_ERROR(
         i2c_new_master_bus(&bus_config, &i2c_bus_handle),
         TAG,
         "Could not initialize I2C bus"
     );

     esp_err_t err = i2c_master_probe(
         i2c_bus_handle,
         TIC_I2C_ADDRESS,
         100
     );

     if (err != ESP_OK)
     {
         ESP_LOGE(
             TAG,
             "Tic not found at address 0x%02X: %s",
             TIC_I2C_ADDRESS,
             esp_err_to_name(err)
         );

         return err;
     }

     ESP_LOGI(
         TAG,
         "Tic detected at address 0x%02X",
         TIC_I2C_ADDRESS
     );

     const i2c_device_config_t device_config = {
         .dev_addr_length = I2C_ADDR_BIT_LEN_7,
         .device_address = TIC_I2C_ADDRESS,
         .scl_speed_hz = TIC_I2C_FREQUENCY_HZ,
     };

     ESP_RETURN_ON_ERROR(
         i2c_master_bus_add_device(
             i2c_bus_handle,
             &device_config,
             &tic_device_handle
         ),
         TAG,
         "Could not add Tic to I2C bus"
     );

     ESP_LOGI(TAG, "Tic driver initialized");

     return ESP_OK;
 }

 esp_err_t tic_driver_set_target_position(int32_t target_position)
 {
     uint8_t command[5];

     command[0] = TIC_CMD_SET_TARGET_POSITION;
     command[1] = (uint8_t)(target_position);
     command[2] = (uint8_t)(target_position >> 8);
     command[3] = (uint8_t)(target_position >> 16);
     command[4] = (uint8_t)(target_position >> 24);

     esp_err_t err = i2c_master_transmit(
         tic_device_handle,
         command,
         sizeof(command),
         TIC_I2C_TIMEOUT_MS
     );

     if (err != ESP_OK)
     {
         ESP_LOGE(
             TAG,
             "Could not set target position: %s",
             esp_err_to_name(err)
         );

         return err;
     }

     ESP_LOGI(
         TAG,
         "Target position set to %" PRId32,
         target_position
     );

     return ESP_OK;
 }
 esp_err_t tic_driver_halt_and_hold(void)
 {
     ESP_LOGI(TAG, "Tic halt and hold [simulated]");
     return ESP_OK;
 }

 
 static esp_err_t tic_get_variables(
     uint8_t offset,
     uint8_t *data,
     size_t length
 )
 {
     if (data == NULL || length == 0 || length > 15)
     {
         return ESP_ERR_INVALID_ARG;
     }

     const uint8_t command[] = {
         TIC_CMD_GET_VARIABLE,
         offset
     };

     return i2c_master_transmit_receive(
         tic_device_handle,
         command,
         sizeof(command),
         data,
         length,
         TIC_I2C_TIMEOUT_MS
     );
 }
 
 static uint16_t read_u16_le(const uint8_t *data)
 {
     return
         ((uint16_t)data[0]) |
         ((uint16_t)data[1] << 8);
 }

 static int32_t read_i32_le(const uint8_t *data)
 {
     uint32_t value =
         ((uint32_t)data[0]) |
         ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);

     return (int32_t)value;
 }
 
 esp_err_t tic_driver_get_status(tic_status_t *status)
 {
     if (status == NULL)
     {
         return ESP_ERR_INVALID_ARG;
     }

     uint8_t general_status[4];

     esp_err_t err = tic_get_variables(
         TIC_VAR_OPERATION_STATE,
         general_status,
         sizeof(general_status)
     );

     if (err != ESP_OK)
     {
         ESP_LOGE(
             TAG,
             "Could not read Tic general status: %s",
             esp_err_to_name(err)
         );

         return err;
     }

     status->operation_state = general_status[0];

     uint8_t flags = general_status[1];

     status->energized =
         (flags & (1U << 0)) != 0;

     status->position_uncertain =
         (flags & (1U << 1)) != 0;

     status->forward_limit_active =
         (flags & (1U << 2)) != 0;

     status->reverse_limit_active =
         (flags & (1U << 3)) != 0;

     status->homing_active =
         (flags & (1U << 4)) != 0;

     status->error_status =
         read_u16_le(&general_status[2]);

     uint8_t position_data[4];

     err = tic_get_variables(
         TIC_VAR_CURRENT_POSITION,
         position_data,
         sizeof(position_data)
     );

     if (err != ESP_OK)
     {
         ESP_LOGE(
             TAG,
             "Could not read Tic position: %s",
             esp_err_to_name(err)
         );

         return err;
     }

     status->current_position =
         read_i32_le(position_data);

     return ESP_OK;
 }
 
 static esp_err_t tic_send_command(uint8_t command)
 {
     return i2c_master_transmit(
         tic_device_handle,
         &command,
         sizeof(command),
         TIC_I2C_TIMEOUT_MS
     );
 }
 
 static esp_err_t tic_send_quick_command(uint8_t command)
 {
     esp_err_t err = i2c_master_transmit(
         tic_device_handle,
         &command,
         sizeof(command),
         TIC_I2C_TIMEOUT_MS
     );

     if (err != ESP_OK)
     {
         ESP_LOGE(
             TAG,
             "Could not send Tic command 0x%02X: %s",
             command,
             esp_err_to_name(err)
         );
     }

     return err;
 }
 
 esp_err_t tic_driver_energize(void)
 {
     esp_err_t err =
         tic_send_quick_command(TIC_CMD_ENERGIZE);

     if (err == ESP_OK)
     {
         ESP_LOGI(TAG, "Tic energized");
     }

     return err;
 }

 esp_err_t tic_driver_exit_safe_start(void)
 {
     esp_err_t err =
         tic_send_quick_command(TIC_CMD_EXIT_SAFE_START);

     if (err == ESP_OK)
     {
         ESP_LOGI(TAG, "Tic exited safe start");
     }

     return err;
 }

 esp_err_t tic_driver_deenergize(void)
 {
     esp_err_t err =
         tic_send_quick_command(TIC_CMD_DEENERGIZE);

     if (err == ESP_OK)
     {
         ESP_LOGI(TAG, "Tic de-energized");
     }

     return err;
 }
 
 esp_err_t tic_driver_go_home(tic_home_direction_t direction)
 {
     if (direction != TIC_HOME_REVERSE &&
         direction != TIC_HOME_FORWARD)
     {
         return ESP_ERR_INVALID_ARG;
     }

     const uint8_t command[] = {
         TIC_CMD_GO_HOME,
         (uint8_t)direction
     };

     esp_err_t err = i2c_master_transmit(
         tic_device_handle,
         command,
         sizeof(command),
         TIC_I2C_TIMEOUT_MS
     );

     if (err != ESP_OK)
     {
         ESP_LOGE(
             TAG,
             "Could not start homing: %s",
             esp_err_to_name(err)
         );

         return err;
     }

     ESP_LOGI(
         TAG,
         "Homing started in %s direction",
         direction == TIC_HOME_FORWARD ? "forward" : "reverse"
     );

     return ESP_OK;
 }
 
 esp_err_t tic_driver_reset_command_timeout(void)
 {
     esp_err_t err = tic_send_quick_command(
         TIC_CMD_RESET_COMMAND_TIMEOUT
     );

     if (err == ESP_OK)
     {
         ESP_LOGI(TAG, "Command timeout reset");
     }

     return err;
 }