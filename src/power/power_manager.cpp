#include "power_manager.h"

#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char* TAG = "POWER";

PowerManager::PowerManager()
{
    currentState = PowerState::ACTIVE;
}

void PowerManager::begin()
{
    // Bus Low Enable Jumper
    gpio_config_t bus_low_cfg = {};
    bus_low_cfg.pin_bit_mask = (1ULL << PIN_BUS_LOW);
    bus_low_cfg.mode = GPIO_MODE_INPUT;
    bus_low_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    bus_low_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
    bus_low_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&bus_low_cfg);

    // Sensor Power
    gpio_config_t sensor_pwr_cfg = {};
    sensor_pwr_cfg.pin_bit_mask = (1ULL << PIN_SENSOR_POWER);
    sensor_pwr_cfg.mode = GPIO_MODE_OUTPUT;
    sensor_pwr_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    sensor_pwr_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    sensor_pwr_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&sensor_pwr_cfg);

    disableSensorPower();

    // Configure Automatic Light Sleep
    esp_pm_config_t pm_config = {};
    pm_config.max_freq_mhz = 160;
    pm_config.min_freq_mhz = 10;
    pm_config.light_sleep_enable = true;
    esp_pm_configure(&pm_config);

    currentState = PowerState::ACTIVE;
}

void PowerManager::setSensorPower(bool enabled)
{
    gpio_set_level(PIN_SENSOR_POWER, enabled ? 1 : 0);
}

void PowerManager::enableSensorPower()
{
    setSensorPower(true);
}

void PowerManager::disableSensorPower()
{
    setSensorPower(false);
}

bool PowerManager::isBusLowEnabled()
{
    return gpio_get_level(PIN_BUS_LOW) == 0;
}

bool PowerManager::shouldEnterDeepSleep()
{
    // Jumper removed -> GPIO HIGH
    // Device should sleep
    return !isBusLowEnabled();
}

PowerState PowerManager::getPowerState() const
{
    return currentState;
}

esp_sleep_wakeup_cause_t PowerManager::getWakeupCause()
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return esp_sleep_get_wakeup_cause();
#pragma GCC diagnostic pop
}

void PowerManager::enterDeepSleep()
{
    currentState = PowerState::DEEP_SLEEP;

    // Turn off sensors
    disableSensorPower();

    ESP_LOGI(TAG, "Configuring Deep Sleep");

    // Clear previous wake sources
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // Wake when jumper is removed (GPIO goes LOW via pulldown)
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_BUS_LOW, ESP_EXT1_WAKEUP_ANY_LOW);

    ESP_LOGI(TAG, "Entering Deep Sleep");

    fflush(stdout);

    vTaskDelay(pdMS_TO_TICKS(100));

    esp_deep_sleep_start();
}

static adc_oneshot_unit_handle_t adc1_handle;

uint8_t PowerManager::getBatteryPercentage()
{
    if (!adcInitialized) {
        adc_oneshot_unit_init_cfg_t init_config = {};
        init_config.unit_id = ADC_UNIT_1;
        init_config.ulp_mode = ADC_ULP_MODE_DISABLE;
        adc_oneshot_new_unit(&init_config, &adc1_handle);

        adc_oneshot_chan_cfg_t config = {};
        config.atten = ADC_ATTEN_DB_12;
        config.bitwidth = ADC_BITWIDTH_DEFAULT;
        adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &config); // GPIO0 is ADC1_CH0
        adcInitialized = true;
    }

    int raw_val;
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw_val);
    
    // Convert raw reading to voltage. With 12dB attenuation, max voltage is roughly 3.3V, but usually voltage dividers are used.
    // For a standard 1/2 voltage divider on a 4.2V battery, max voltage at pin is 2.1V.
    // Assuming 3.3V reference and 12-bit resolution (4095).
    // Voltage = (raw_val / 4095.0) * 3.3 * 2 (if using 1/2 divider).
    // The user config thresholds: BATTERY_LOW_VOLTAGE (3.5), BATTERY_FULL_VOLTAGE (4.2).
    
    float voltage = ((float)raw_val / 4095.0f) * 3.3f * 2.0f; 
    
    if (voltage >= BATTERY_FULL_VOLTAGE) return 100;
    if (voltage <= BATTERY_LOW_VOLTAGE) return 0;
    
    return (uint8_t)(((voltage - BATTERY_LOW_VOLTAGE) / (BATTERY_FULL_VOLTAGE - BATTERY_LOW_VOLTAGE)) * 100.0f);
}