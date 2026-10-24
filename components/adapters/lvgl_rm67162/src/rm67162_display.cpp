#include "tesla_headup/adapters/lvgl_rm67162/rm67162_display.hpp"

// ESP-IDF 5.4 uses anonymous structs in its C LCD color declarations. They
// are valid for ESP-IDF, but trigger -Wpedantic when included from C++.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#pragma GCC diagnostic pop

#include <algorithm>
#include <cstdint>

#if !LV_DRAW_SW_SUPPORT_RGB565_SWAPPED
#error "The RM67162 adapter requires LVGL RGB565-swapped rendering support"
#endif

namespace tesla_headup::platform {
namespace {

constexpr char tag[] = "tesla_headup_display";
// LILYGO's H619 reference implementation uses the ESP32-S3 SPI2 peripheral
// and manually controls CS for the complete QSPI pixel transaction. SPI3 is
// not interchangeable here: although the GPIO matrix can route the same pins,
// the known-good board timing and transaction path use SPI2_HOST.
constexpr auto spi_host = SPI2_HOST;
constexpr std::size_t bytes_per_pixel = 2;
constexpr std::size_t draw_buffer_size =
    static_cast<std::size_t>(Rm67162Display::logical_width) *
    CONFIG_TESLA_HEADUP_DISPLAY_BUFFER_LINES * bytes_per_pixel;
constexpr std::size_t transfer_chunk_pixels = 16'384;
constexpr std::size_t transfer_chunk_bytes =
    transfer_chunk_pixels * bytes_per_pixel;
constexpr std::uint8_t qspi_command_write = 0x02;
constexpr std::uint8_t qspi_pixel_write = 0x32;
constexpr std::uint16_t sleep_out = 0x1100;
constexpr std::uint16_t display_on = 0x2900;
constexpr std::uint16_t column_address_set = 0x2A00;
constexpr std::uint16_t row_address_set = 0x2B00;
constexpr std::uint16_t memory_write = 0x2C00;
constexpr std::uint16_t memory_access_control = 0x3600;
constexpr std::uint16_t interface_pixel_format = 0x3A00;
constexpr std::uint16_t write_display_brightness = 0x5100;
constexpr std::uint8_t rgb565_pixel_format = 0x55;
constexpr std::uint8_t landscape_orientation = 0x60;
constexpr std::uint8_t mirrored_landscape_orientation = 0xE0;
constexpr int initialization_attempts = 3;
#if CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
constexpr std::uint64_t performance_report_interval_us = 1'000'000U;
#else
constexpr std::uint64_t performance_report_interval_us = 5'000'000U;
#endif
#if CONFIG_TESLA_HEADUP_DISPLAY_PROJECTION_MIRRORED
constexpr bool projection_mirrored_by_default = true;
#else
constexpr bool projection_mirrored_by_default = false;
#endif
#if CONFIG_TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE
constexpr char dashboard_layout[] = "road_scene";
#else
constexpr char dashboard_layout[] = "true_hud";
#endif

std::uint32_t lvgl_milliseconds() {
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1'000);
}

std::uint32_t microseconds_32() noexcept {
    return static_cast<std::uint32_t>(esp_timer_get_time());
}

void update_maximum(
    std::atomic<std::uint32_t>& maximum,
    std::uint32_t candidate) noexcept {
    auto previous = maximum.load(std::memory_order_relaxed);
    while (previous < candidate &&
           !maximum.compare_exchange_weak(
               previous,
               candidate,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

}  // namespace

esp_err_t Rm67162Display::initialize() noexcept {
    if (ready()) {
        return ESP_OK;
    }

    ESP_LOGI(tag, "Powering the RM67162 display");
    gpio_config_t power_config{};
    power_config.pin_bit_mask = 1ULL << CONFIG_TESLA_HEADUP_DISPLAY_PIN_POWER;
    power_config.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&power_config), tag, "power GPIO");
    ESP_RETURN_ON_ERROR(
        gpio_set_level(
            static_cast<gpio_num_t>(CONFIG_TESLA_HEADUP_DISPLAY_PIN_POWER), 1),
        tag,
        "display power");
    vTaskDelay(pdMS_TO_TICKS(500));

    gpio_config_t control_config{};
    control_config.pin_bit_mask =
        (1ULL << CONFIG_TESLA_HEADUP_DISPLAY_PIN_CS) |
        (1ULL << CONFIG_TESLA_HEADUP_DISPLAY_PIN_RESET);
    control_config.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&control_config), tag, "display controls");
    ESP_RETURN_ON_ERROR(
        gpio_set_level(
            static_cast<gpio_num_t>(CONFIG_TESLA_HEADUP_DISPLAY_PIN_CS), 1),
        tag,
        "display chip select");

    // The RM67162 reset input is active low. Match LILYGO's H619 sequence and
    // finish high so that the panel cannot remain held in reset.
    ESP_RETURN_ON_ERROR(
        gpio_set_level(
            static_cast<gpio_num_t>(CONFIG_TESLA_HEADUP_DISPLAY_PIN_RESET), 1),
        tag,
        "display reset high");
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_RETURN_ON_ERROR(
        gpio_set_level(
            static_cast<gpio_num_t>(CONFIG_TESLA_HEADUP_DISPLAY_PIN_RESET), 0),
        tag,
        "display reset low");
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_RETURN_ON_ERROR(
        gpio_set_level(
            static_cast<gpio_num_t>(CONFIG_TESLA_HEADUP_DISPLAY_PIN_RESET), 1),
        tag,
        "display reset release");
    vTaskDelay(pdMS_TO_TICKS(200));

    spi_bus_config_t bus_config{};
    bus_config.data0_io_num = CONFIG_TESLA_HEADUP_DISPLAY_PIN_DATA0;
    bus_config.data1_io_num = CONFIG_TESLA_HEADUP_DISPLAY_PIN_DATA1;
    bus_config.data2_io_num = CONFIG_TESLA_HEADUP_DISPLAY_PIN_DATA2;
    bus_config.data3_io_num = CONFIG_TESLA_HEADUP_DISPLAY_PIN_DATA3;
    bus_config.sclk_io_num = CONFIG_TESLA_HEADUP_DISPLAY_PIN_SCK;
    bus_config.max_transfer_sz = transfer_chunk_bytes + 8;
    bus_config.flags = SPICOMMON_BUSFLAG_MASTER |
                       SPICOMMON_BUSFLAG_GPIO_PINS;
    ESP_RETURN_ON_ERROR(
        spi_bus_initialize(spi_host, &bus_config, SPI_DMA_CH_AUTO),
        tag,
        "QSPI bus");

    spi_device_interface_config_t device_config{};
    device_config.command_bits = 8;
    device_config.address_bits = 24;
    device_config.mode = 0;
    device_config.clock_speed_hz = CONFIG_TESLA_HEADUP_DISPLAY_SPI_CLOCK_HZ;
    device_config.spics_io_num = -1;
    device_config.flags = SPI_DEVICE_HALFDUPLEX;
    device_config.queue_size = 17;
    spi_device_handle_t spi_device = nullptr;
    ESP_RETURN_ON_ERROR(
        spi_bus_add_device(spi_host, &device_config, &spi_device),
        tag,
        "RM67162 SPI device");
    spi_device_ = spi_device;

    // LILYGO deliberately repeats this short initialization sequence because
    // some RM67162 panels occasionally ignore a command immediately after
    // reset. Keep the exact command ordering used by the H619 reference.
    for (int attempt = 0; attempt < initialization_attempts; ++attempt) {
        ESP_RETURN_ON_ERROR(write_command(sleep_out), tag, "sleep out");
        vTaskDelay(pdMS_TO_TICKS(120));
        ESP_RETURN_ON_ERROR(
            write_command(
                interface_pixel_format,
                &rgb565_pixel_format,
                sizeof(rgb565_pixel_format)),
            tag,
            "RGB565 pixel format");
        const std::uint8_t brightness_off = 0;
        ESP_RETURN_ON_ERROR(
            write_command(
                write_display_brightness,
                &brightness_off,
                sizeof(brightness_off)),
            tag,
            "display brightness off");
        ESP_RETURN_ON_ERROR(write_command(display_on), tag, "display on");
        vTaskDelay(pdMS_TO_TICKS(120));
        const std::uint8_t initial_brightness = 0xD0;
        ESP_RETURN_ON_ERROR(
            write_command(
                write_display_brightness,
                &initial_brightness,
                sizeof(initial_brightness)),
            tag,
            "initial display brightness");
        ESP_RETURN_ON_ERROR(
            write_command(
                memory_access_control,
                &landscape_orientation,
                sizeof(landscape_orientation)),
            tag,
            "landscape orientation");
    }

    draw_buffer_a_ = heap_caps_malloc(draw_buffer_size, MALLOC_CAP_DMA);
    // write_pixels() completes its polling transfer before flush_ready(). A
    // second buffer cannot overlap rendering with DMA on this synchronous path.
    if (draw_buffer_a_ == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    lv_init();
    lv_tick_set_cb(lvgl_milliseconds);
    // MADCTL has already put the controller into 536x240 landscape mode.
    // Expose those physical coordinates directly to LVGL. Marking a 240x536
    // display as rotated only swaps LVGL's logical resolution; it does not
    // rotate partial draw buffers for the flush callback.
    lvgl_display_ = lv_display_create(logical_width, logical_height);
    if (lvgl_display_ == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    lv_display_set_color_format(
        lvgl_display_, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_user_data(lvgl_display_, this);
    lv_display_set_flush_cb(lvgl_display_, Rm67162Display::flush);
    lv_display_add_event_cb(
        lvgl_display_, performance_event, LV_EVENT_ALL, this);
    last_performance_report_us_ =
        static_cast<std::uint64_t>(esp_timer_get_time());

    lv_display_set_buffers(
        lvgl_display_,
        draw_buffer_a_,
        nullptr,
        draw_buffer_size,
        LV_DISPLAY_RENDER_MODE_PARTIAL);

    if (!canvas_.initialize(*lvgl_display_)) {
        return ESP_ERR_NO_MEM;
    }
    set_projection_mirrored(
        projection_configured_ ? projection_mirrored_
                               : projection_mirrored_by_default);
    ESP_RETURN_ON_ERROR(
        set_brightness_percent(brightness_percent_),
        tag,
        "display brightness");
    ESP_LOGI(
        tag,
        "Display ready: %dx%d, layout=%s, LILYGO polling QSPI, "
        "one %u-byte DMA buffer",
        logical_width,
        logical_height,
        dashboard_layout,
        static_cast<unsigned>(draw_buffer_size));
    return ESP_OK;
}

void Rm67162Display::flush(
    lv_display_t* display,
    const lv_area_t* area,
    std::uint8_t* pixels) {
    auto* adapter = static_cast<Rm67162Display*>(
        lv_display_get_user_data(display));
    if (adapter == nullptr || area == nullptr || pixels == nullptr) {
        ESP_LOGE(tag, "Display flush received invalid arguments");
        lv_display_flush_ready(display);
        return;
    }

#if CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
    static std::atomic<std::uint32_t> diagnostic_flush_count{0};
    const auto flush_number = diagnostic_flush_count.fetch_add(
        1, std::memory_order_relaxed);
    if (flush_number < 12) {
        ESP_LOGI(
            tag,
            "LVGL flush %lu: (%ld,%ld)-(%ld,%ld), first=%02x%02x",
            static_cast<unsigned long>(flush_number + 1),
            static_cast<long>(area->x1),
            static_cast<long>(area->y1),
            static_cast<long>(area->x2),
            static_cast<long>(area->y2),
            static_cast<unsigned>(pixels[0]),
            static_cast<unsigned>(pixels[1]));
    }
#endif

    const esp_err_t result = adapter->write_pixels(*area, pixels);
    if (result != ESP_OK) {
        ESP_LOGE(tag, "Display flush failed: %s", esp_err_to_name(result));
    }
    lv_display_flush_ready(display);
}

esp_err_t Rm67162Display::write_command(
    std::uint16_t address,
    const void* parameters,
    std::size_t parameter_size) noexcept {
    if (spi_device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    spi_transaction_t transaction{};
    transaction.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    transaction.cmd = qspi_command_write;
    transaction.addr = address;
    transaction.tx_buffer = parameters;
    transaction.length = parameter_size * 8U;

    const auto chip_select =
        static_cast<gpio_num_t>(CONFIG_TESLA_HEADUP_DISPLAY_PIN_CS);
    ESP_RETURN_ON_ERROR(gpio_set_level(chip_select, 0), tag, "select display");
    const esp_err_t result = spi_device_polling_transmit(
        static_cast<spi_device_handle_t>(spi_device_), &transaction);
    const esp_err_t deselect_result = gpio_set_level(chip_select, 1);
    if (result != ESP_OK) {
        return result;
    }
    return deselect_result;
}

esp_err_t Rm67162Display::write_pixels(
    const lv_area_t& area,
    const std::uint8_t* pixels) noexcept {
    if (spi_device_ == nullptr || pixels == nullptr || area.x1 < 0 ||
        area.y1 < 0 || area.x2 < area.x1 || area.y2 < area.y1 ||
        area.x2 >= logical_width || area.y2 >= logical_height) {
        return ESP_ERR_INVALID_ARG;
    }

    const std::uint16_t x_start = static_cast<std::uint16_t>(area.x1);
    const std::uint16_t x_end = static_cast<std::uint16_t>(area.x2);
    const std::uint16_t y_start = static_cast<std::uint16_t>(area.y1);
    const std::uint16_t y_end = static_cast<std::uint16_t>(area.y2);
    const std::uint8_t columns[]{
        static_cast<std::uint8_t>(x_start >> 8U),
        static_cast<std::uint8_t>(x_start),
        static_cast<std::uint8_t>(x_end >> 8U),
        static_cast<std::uint8_t>(x_end),
    };
    const std::uint8_t rows[]{
        static_cast<std::uint8_t>(y_start >> 8U),
        static_cast<std::uint8_t>(y_start),
        static_cast<std::uint8_t>(y_end >> 8U),
        static_cast<std::uint8_t>(y_end),
    };
    ESP_RETURN_ON_ERROR(
        write_command(column_address_set, columns, sizeof(columns)),
        tag,
        "column window");
    ESP_RETURN_ON_ERROR(
        write_command(row_address_set, rows, sizeof(rows)),
        tag,
        "row window");
    ESP_RETURN_ON_ERROR(write_command(memory_write), tag, "memory write");

    std::size_t remaining =
        static_cast<std::size_t>(area.x2 - area.x1 + 1) *
        static_cast<std::size_t>(area.y2 - area.y1 + 1);
    const auto* source = reinterpret_cast<const std::uint16_t*>(pixels);
    bool first_chunk = true;
    const auto chip_select =
        static_cast<gpio_num_t>(CONFIG_TESLA_HEADUP_DISPLAY_PIN_CS);
    ESP_RETURN_ON_ERROR(gpio_set_level(chip_select, 0), tag, "select display");

    esp_err_t result = ESP_OK;
    while (remaining > 0) {
        const std::size_t chunk =
            std::min(remaining, transfer_chunk_pixels);
        spi_transaction_ext_t transaction{};
        transaction.base.flags = SPI_TRANS_MODE_QIO;
        if (first_chunk) {
            transaction.base.cmd = qspi_pixel_write;
            transaction.base.addr = memory_write;
            first_chunk = false;
        } else {
            transaction.base.flags |= SPI_TRANS_VARIABLE_CMD |
                                      SPI_TRANS_VARIABLE_ADDR |
                                      SPI_TRANS_VARIABLE_DUMMY;
            transaction.command_bits = 0;
            transaction.address_bits = 0;
            transaction.dummy_bits = 0;
        }
        transaction.base.tx_buffer = source;
        transaction.base.length = chunk * bytes_per_pixel * 8U;
        result = spi_device_polling_transmit(
            static_cast<spi_device_handle_t>(spi_device_),
            reinterpret_cast<spi_transaction_t*>(&transaction));
        if (result != ESP_OK) {
            break;
        }
        remaining -= chunk;
        source += chunk;
    }

    const esp_err_t deselect_result = gpio_set_level(chip_select, 1);
    if (result != ESP_OK) {
        return result;
    }
    return deselect_result;
}

void Rm67162Display::process() noexcept {
    if (ready()) {
        (void)lv_timer_handler();
        report_performance();
    }
}

void Rm67162Display::performance_event(lv_event_t* event) {
    auto* display = static_cast<Rm67162Display*>(
        lv_event_get_user_data(event));
    if (display == nullptr) {
        return;
    }

    const auto timestamp = microseconds_32();
    switch (lv_event_get_code(event)) {
    case LV_EVENT_REFR_START:
        display->refresh_started_at_us_.store(
            timestamp, std::memory_order_relaxed);
        break;
    case LV_EVENT_REFR_READY: {
        const auto elapsed = timestamp - display->refresh_started_at_us_.load(
                                             std::memory_order_relaxed);
        display->refresh_count_.fetch_add(1, std::memory_order_relaxed);
        display->refresh_total_us_.fetch_add(elapsed, std::memory_order_relaxed);
        update_maximum(display->refresh_max_us_, elapsed);
        break;
    }
    case LV_EVENT_FLUSH_START:
        display->flush_started_at_us_.store(
            timestamp, std::memory_order_relaxed);
        break;
    case LV_EVENT_FLUSH_FINISH: {
        const auto elapsed = timestamp - display->flush_started_at_us_.load(
                                             std::memory_order_relaxed);
        display->flush_count_.fetch_add(1, std::memory_order_relaxed);
        display->flush_total_us_.fetch_add(elapsed, std::memory_order_relaxed);
        update_maximum(display->flush_max_us_, elapsed);
        break;
    }
    default:
        break;
    }
}

void Rm67162Display::report_performance() noexcept {
    const auto timestamp = static_cast<std::uint64_t>(esp_timer_get_time());
    const auto elapsed = timestamp - last_performance_report_us_;
    if (elapsed < performance_report_interval_us) {
        return;
    }
    last_performance_report_us_ = timestamp;

    const auto refreshes = refresh_count_.exchange(0, std::memory_order_relaxed);
    const auto refresh_total =
        refresh_total_us_.exchange(0, std::memory_order_relaxed);
    const auto refresh_max =
        refresh_max_us_.exchange(0, std::memory_order_relaxed);
    const auto flushes = flush_count_.exchange(0, std::memory_order_relaxed);
    const auto flush_total =
        flush_total_us_.exchange(0, std::memory_order_relaxed);
    const auto flush_max = flush_max_us_.exchange(0, std::memory_order_relaxed);
    const auto fps = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(refreshes) *
         1'000'000U) /
        elapsed);

    lv_mem_monitor_t lvgl_memory{};
    lv_mem_monitor(&lvgl_memory);
    ESP_LOGI(
        tag,
        "Display performance: fps=%lu refresh_avg=%luus refresh_max=%luus "
        "flushes=%lu flush_avg=%luus flush_max=%luus "
        "free_internal=%u "
        "min_internal=%u largest_internal=%u free_psram=%u min_psram=%u "
        "lvgl_free=%u lvgl_largest=%u lvgl_frag=%u%% stack_free=%u",
        static_cast<unsigned long>(fps),
        static_cast<unsigned long>(
            refreshes == 0 ? 0 : refresh_total / refreshes),
        static_cast<unsigned long>(refresh_max),
        static_cast<unsigned long>(flushes),
        static_cast<unsigned long>(flushes == 0 ? 0 : flush_total / flushes),
        static_cast<unsigned long>(flush_max),
        static_cast<unsigned>(heap_caps_get_free_size(
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_minimum_free_size(
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
        static_cast<unsigned>(
            heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)),
        static_cast<unsigned>(lvgl_memory.free_size),
        static_cast<unsigned>(lvgl_memory.free_biggest_size),
        static_cast<unsigned>(lvgl_memory.frag_pct),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

void Rm67162Display::set_projection_mirrored(bool mirrored) noexcept {
    projection_configured_ = true;
    if (spi_device_ == nullptr) {
        projection_mirrored_ = mirrored;
        return;
    }

    const std::uint8_t orientation =
        mirrored ? mirrored_landscape_orientation : landscape_orientation;
    if (write_command(
            memory_access_control,
            &orientation,
            sizeof(orientation)) == ESP_OK) {
        projection_mirrored_ = mirrored;
        if (lvgl_display_ != nullptr) {
            lv_obj_invalidate(lv_display_get_screen_active(lvgl_display_));
        }
    }
}

bool Rm67162Display::projection_mirrored() const noexcept {
    return projection_mirrored_;
}

esp_err_t Rm67162Display::set_brightness_percent(
    std::uint8_t percent) noexcept {
    if (percent == 0 || percent > 100) {
        return ESP_ERR_INVALID_ARG;
    }

    brightness_percent_ = percent;
    if (spi_device_ == nullptr) {
        return ESP_OK;
    }

    const auto brightness = static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(percent) * 255U + 50U) / 100U);
    return write_command(
        write_display_brightness, &brightness, sizeof(brightness));
}

std::uint8_t Rm67162Display::brightness_percent() const noexcept {
    return brightness_percent_;
}

bool Rm67162Display::ready() const noexcept {
    return spi_device_ != nullptr && lvgl_display_ != nullptr && canvas_.ready();
}

LvglDashboardCanvas& Rm67162Display::canvas() noexcept {
    return canvas_;
}

}  // namespace tesla_headup::platform
