# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS
     "main.c"
     "motor_control.c"
     "motor_commands.c"
     "console_app.c"
     "tic_driver.c"    
    INCLUDE_DIRS		# optional, add here public include directories
    "."        
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
    	esp_driver_i2c
    	#driver
    	console
)
