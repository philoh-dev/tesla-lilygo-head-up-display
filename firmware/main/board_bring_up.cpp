#include "board.hpp"
#include "sdkconfig.h"

#include "esp_err.h"
// ESP-IDF's C flash HAL uses anonymous structs. Suppress that vendor-header
// diagnostic locally; keep -Wpedantic/-Werror enabled for our own code.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "esp_flash.h"
#pragma GCC diagnostic pop
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"

#include <cstdint>
#include <cstdlib>

extern "C" void app_main() {
    namespace board = tesla_headup::board::devkitc_n32r16v;
    constexpr char tag[] = "tesla_headup_board";
    std::uint32_t flash_size = 0;
    ESP_ERROR_CHECK(esp_flash_get_size(nullptr, &flash_size));
    ESP_LOGI(tag, "Profile: devkitc_n32r16v (memory-only bring-up)");
    ESP_LOGI(tag, "Flash: %lu bytes; PSRAM: %lu bytes",
             static_cast<unsigned long>(flash_size),
             static_cast<unsigned long>(esp_psram_get_size()));
    if (flash_size != board::flash_bytes || !esp_psram_is_initialized() ||
        esp_psram_get_size() != board::psram_bytes) {
        ESP_LOGE(tag, "Expected 32 MiB flash and 16 MiB PSRAM; check board variant/config");
        std::abort();
    }
    // Check that initialized PSRAM is usable by future RGB framebuffers.
    constexpr std::size_t framebuffer_pair_bytes = 800U * 480U * 2U * 2U;
    void* framebuffers = heap_caps_malloc(
        framebuffer_pair_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (framebuffers == nullptr) {
        ESP_LOGE(tag, "Cannot reserve two RGB565 framebuffer equivalents");
        std::abort();
    }
    heap_caps_free(framebuffers);
    ESP_LOGI(tag, "Memory checks passed; two RGB565 framebuffer equivalents fit");
    ESP_LOGI(tag, "BLE and display disabled. No RGB or backlight GPIOs driven.");
}
