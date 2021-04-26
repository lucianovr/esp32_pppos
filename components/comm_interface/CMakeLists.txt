set( srcs "c_files/wifi_if.c" )

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES modem )
