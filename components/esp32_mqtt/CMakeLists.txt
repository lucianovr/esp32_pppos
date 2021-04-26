set( srcs "c_files/esp32_mqtt.c" )

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES mqtt)
