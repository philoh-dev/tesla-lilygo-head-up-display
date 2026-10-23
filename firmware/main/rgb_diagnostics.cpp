#include "sdkconfig.h"
#include "tesla_headup/adapters/lvgl_rgb/newhaven_rgb_display.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if !CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS || !CONFIG_TESLA_HEADUP_DISPLAY_RGB
#error "RGB diagnostic entry point requires the RGB diagnostic configuration"
#endif

extern "C" void app_main() {
    constexpr char tag[] = "tesla_headup_rgb_diag";
    ESP_LOGW(tag, "RGB diagnostics: BLE/NVS/dashboard disabled; patterns repeat every 24 seconds");
    ESP_LOGW(tag, "Use only after regulator, PWM level-shifter and FPC checks are resolved");
    static tesla_headup::platform::NewhavenRgbDisplay display;
#if CONFIG_TESLA_HEADUP_RGB_DIAGNOSTIC_MIRRORED
    display.set_projection_mirrored(true);
#else
    display.set_projection_mirrored(false);
#endif
    ESP_ERROR_CHECK(display.set_brightness_percent(CONFIG_TESLA_HEADUP_RGB_DIAGNOSTIC_BRIGHTNESS));
    ESP_ERROR_CHECK(display.initialize());
    tesla_headup::platform::RgbDiagnosticSequence sequence;
    while (display.ready()) {
        const auto now = static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
        if (sequence.tick(now)) {
            display.set_diagnostic_pattern(sequence.pattern());
            const auto name = tesla_headup::platform::RgbDiagnostics::name(sequence.pattern());
            const auto origin = display.dashboard_position();
            ESP_LOGI(tag, "Pattern=%.*s origin=(%d,%d) mirrored=%d brightness=%u%%",
                static_cast<int>(name.size()), name.data(), origin.x, origin.y,
                display.projection_mirrored(), static_cast<unsigned>(display.brightness_percent()));
        }
        display.process();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGE(tag, "Diagnostic output failed; backlight disabled. Restart after investigating the error.");
    display.shutdown();
}
