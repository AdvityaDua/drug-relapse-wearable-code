idf_component_register(
    SRC_DIRS "." "wifi" "ble" "commands" "power" "sensors" "storage"
    INCLUDE_DIRS "." "../include"
    REQUIRES esp_wifi lwip driver esp_pm esp_timer bt
)
