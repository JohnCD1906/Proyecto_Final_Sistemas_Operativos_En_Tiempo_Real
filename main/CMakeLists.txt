idf_component_register(SRCS "main.c" "swpwm.c" "touch_pad.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gptimer esp_driver_gpio driver)