/*
 * console_app.c
 *
 *  Created on: 2 ago 2026
 *      Author: agusn
 */

 #include "console_app.h"

 #include "esp_console.h"
 #include "esp_err.h"
 #include "esp_log.h"
 #include "sdkconfig.h"
 #include "esp_check.h"

 #include "motor_commands.h"

 static const char *TAG = "CONSOLE";

 static esp_console_repl_t *repl = NULL;

 esp_err_t console_app_init(void)
 {
     esp_console_repl_config_t repl_config =
         ESP_CONSOLE_REPL_CONFIG_DEFAULT();

     repl_config.prompt = "syringe>";
     repl_config.max_cmdline_length = 128;

     ESP_RETURN_ON_ERROR(
         esp_console_register_help_command(),
         TAG,
         "Could not register help command"
     );

     ESP_RETURN_ON_ERROR(
         motor_commands_register(),
         TAG,
         "Could not register motor commands"
     );

 #if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || \
     defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)

     esp_console_dev_uart_config_t device_config =
         ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

     ESP_RETURN_ON_ERROR(
         esp_console_new_repl_uart(
             &device_config,
             &repl_config,
             &repl
         ),
         TAG,
         "Could not create UART REPL"
     );

 #elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)

     esp_console_dev_usb_serial_jtag_config_t device_config =
         ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();

     ESP_RETURN_ON_ERROR(
         esp_console_new_repl_usb_serial_jtag(
             &device_config,
             &repl_config,
             &repl
         ),
         TAG,
         "Could not create USB Serial/JTAG REPL"
     );

 #elif defined(CONFIG_ESP_CONSOLE_USB_CDC)

     esp_console_dev_usb_cdc_config_t device_config =
         ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();

     ESP_RETURN_ON_ERROR(
         esp_console_new_repl_usb_cdc(
             &device_config,
             &repl_config,
             &repl
         ),
         TAG,
         "Could not create USB CDC REPL"
     );

 #else

 #error "No supported console interface configured"

 #endif

     ESP_RETURN_ON_ERROR(
         esp_console_start_repl(repl),
         TAG,
         "Could not start REPL"
     );

     ESP_LOGI(TAG, "Serial console started");

     return ESP_OK;
 }

