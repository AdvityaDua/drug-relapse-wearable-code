idf_component_register(
    SRC_DIRS "." "ble" "commands" "power" "sensors" "storage"
    INCLUDE_DIRS "." "../include"
    REQUIRES bt driver esp_pm esp_timer
)
