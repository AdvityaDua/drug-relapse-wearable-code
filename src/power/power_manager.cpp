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
    // Power Button (D2) — input with internal pull-up
    // Button connects D2 to GND, so idle = HIGH, pressed = LOW
    gpio_config_t btn_cfg = {};
    btn_cfg.pin_bit_mask = (1ULL << PIN_BUTTON);
    btn_cfg.mode = GPIO_MODE_INPUT;
    btn_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    btn_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&btn_cfg);

    // Sensor Power
    gpio_config_t sensor_pwr_cfg = {};
    sensor_pwr_cfg.pin_bit_mask = (1ULL << PIN_SENSOR_POWER);
    sensor_pwr_cfg.mode = GPIO_MODE_OUTPUT;
    sensor_pwr_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    sensor_pwr_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    sensor_pwr_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&sensor_pwr_cfg);

    disableSensorPower();

    // Configure Automatic Light Sleep (disabled during debugging to prevent native USB-Serial/JTAG disconnects)
    esp_pm_config_t pm_config = {};
    pm_config.max_freq_mhz = 160;
    pm_config.min_freq_mhz = 10;
    pm_config.light_sleep_enable = false;
    esp_pm_configure(&pm_config);

    currentState = PowerState::ACTIVE;
}

void PowerManager::startButtonMonitor()
{
    xTaskCreate(buttonMonitorTask, "ButtonMonitor", 2048, this, 1, NULL);
    ESP_LOGI(TAG, "Button monitor started on D2 (GPIO%d)", PIN_BUTTON);
}

void PowerManager::buttonMonitorTask(void* pvParameters)
{
    PowerManager* self = static_cast<PowerManager*>(pvParameters);
    
    // Wait for button to be released at boot (in case user is still holding it from wakeup)
    while (gpio_get_level(PIN_BUTTON) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // Debounce settle after boot

    ESP_LOGI(TAG, "Button monitor ready. Long-press (%lums) to shutdown.", (unsigned long)LONG_PRESS_DURATION_MS);

    while (true) {
        // Wait for button press (LOW)
        if (gpio_get_level(PIN_BUTTON) == 0) {
            // Button is pressed — start timing
            uint32_t pressStart = xTaskGetTickCount();
            bool longPressDetected = false;

            // Keep checking while button is held
            while (gpio_get_level(PIN_BUTTON) == 0) {
                uint32_t elapsed = (xTaskGetTickCount() - pressStart) * portTICK_PERIOD_MS;
                
                if (elapsed >= LONG_PRESS_DURATION_MS) {
                    longPressDetected = true;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            if (longPressDetected) {
                ESP_LOGI(TAG, "Long press detected! Requesting shutdown...");
                self->shutdownRequested = true;
                
                // Wait for button release before allowing sleep
                while (gpio_get_level(PIN_BUTTON) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                vTaskDelay(pdMS_TO_TICKS(100)); // Debounce
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Poll interval
    }
}

bool PowerManager::isShutdownRequested() const
{
    return shutdownRequested;
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

    // Wake when button is pressed (D2 goes LOW via button-to-GND).
    // This API automatically configures internal pull-up/pull-down
    // resistors during deep sleep, so GPIO2 won't float LOW.
    esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(
        1ULL << PIN_BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);

    // Blink LED 4 times to indicate shutdown (LED is active-low: LOW=ON, HIGH=OFF)
    ESP_LOGI(TAG, "Blinking LED 4 times before shutdown...");
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LED, 1); // Start with LED OFF
    for (int i = 0; i < 4; i++) {
        gpio_set_level(PIN_LED, 0); // LED ON
        vTaskDelay(pdMS_TO_TICKS(150));
        gpio_set_level(PIN_LED, 1); // LED OFF
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    ESP_LOGI(TAG, "Entering Deep Sleep — press button to wake");

    fflush(stdout);

    // Extra settle time so any button bounce or noise dissipates
    // before the wakeup source becomes active
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_deep_sleep_start();
}

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool do_calibration = false;

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

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .chan = ADC_CHANNEL_0,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
        do_calibration = (ret == ESP_OK);

        adcInitialized = true;
    }

    uint32_t Vbatt = 0;
    for (int i = 0; i < 16; i++) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw));
        int voltage = 0;
        if (do_calibration) {
            adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage);
        } else {
            voltage = raw; // fallback
        }
        Vbatt += voltage;
    }

    // Apply calibration factor: (3.306V multimeter / 3.270V console)
    float calibration_factor = 3.306f / 3.270f;
    float Vbattf = 2.0f * (float)Vbatt / 16.0f / 1000.0f * calibration_factor;
    float percentage = ((Vbattf - BATTERY_LOW_VOLTAGE) / (BATTERY_FULL_VOLTAGE - BATTERY_LOW_VOLTAGE)) * 100.0f;
    
    if (percentage > 100.0f) percentage = 100.0f;
    if (percentage < 0.0f) percentage = 0.0f;

    ESP_LOGI(TAG, "Voltage: %.3f V, Percentage: %.1f %%", Vbattf, percentage);
    return (uint8_t)percentage;
}