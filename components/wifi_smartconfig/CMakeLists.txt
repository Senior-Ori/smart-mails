idf_component_register(SRCS "wifi_smartconfig.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS ""
                       PRIV_REQUIRES "nvs_flash"
                       REQUIRES "esp_wifi")