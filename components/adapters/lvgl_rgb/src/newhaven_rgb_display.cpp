#include "tesla_headup/adapters/lvgl_rgb/newhaven_rgb_display.hpp"
#include "board.hpp"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include <algorithm>
#include <cstring>

namespace tesla_headup::platform {
namespace {
namespace board = tesla_headup::board::devkitc_n32r16v;
constexpr char tag[] = "tesla_headup_rgb";
#if !CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS
constexpr std::size_t draw_lines = 40;
#endif
constexpr auto pwm_mode = LEDC_LOW_SPEED_MODE;
constexpr auto pwm_channel = LEDC_CHANNEL_0;
constexpr auto pwm_timer = LEDC_TIMER_0;
constexpr std::int64_t handoff_timeout_us = 1'000'000;

#if !CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS
std::uint32_t milliseconds() {
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
}
#endif

bool IRAM_ATTR frame_complete(
    esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t*, void* context) {
    auto& counter = *static_cast<RgbCompletionCounter*>(context);
    portENTER_CRITICAL_ISR(&counter.lock);
    ++counter.value;
    portEXIT_CRITICAL_ISR(&counter.lock);
    return false;
}
}  // namespace

std::uint32_t RgbCompletionCounter::load() noexcept {
    portENTER_CRITICAL(&lock);
    const auto snapshot = value;
    portEXIT_CRITICAL(&lock);
    return snapshot;
}

NewhavenRgbDisplay::NewhavenRgbDisplay() noexcept
    : origin_{surface_.width == 800 ? 0 : CONFIG_TESLA_HEADUP_RGB_ORIGIN_X,
              surface_.height == 480 ? 0 : CONFIG_TESLA_HEADUP_RGB_ORIGIN_Y} {}

NewhavenRgbDisplay::~NewhavenRgbDisplay() { shutdown(); }

esp_err_t NewhavenRgbDisplay::initialize() noexcept {
    if (ready()) {
        return ESP_OK;
    }
    if (failed_ || panel_ != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const auto result = initialize_resources();
    if (result != ESP_OK) {
        shutdown();
    }
    return result;
}

esp_err_t NewhavenRgbDisplay::initialize_resources() noexcept {
    ESP_RETURN_ON_FALSE(RgbFrame::valid(origin_, surface_), ESP_ERR_INVALID_ARG, tag, "viewport");
    ESP_RETURN_ON_FALSE(esp_psram_is_initialized(), ESP_ERR_INVALID_STATE, tag, "PSRAM required");
    // Start dark. DISP is controlled by the RGB peripheral after initialization.
    gpio_config_t pins{};
    pins.pin_bit_mask = (1ULL << board::backlight_pwm_pin) |
                       (1ULL << board::display_enable_pin);
    pins.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&pins), tag, "control GPIOs");
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(board::backlight_pwm_pin), 0), tag, "PWM off");
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(board::display_enable_pin), 0), tag, "DISP off");

    ledc_timer_config_t timer{};
    timer.speed_mode = pwm_mode;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num = pwm_timer;
    timer.freq_hz = 1000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), tag, "PWM timer");
    ledc_channel_config_t channel{};
    channel.gpio_num = board::backlight_pwm_pin;
    channel.speed_mode = pwm_mode;
    channel.channel = pwm_channel;
    channel.timer_sel = pwm_timer;
    channel.duty = 0;
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), tag, "PWM channel");
    pwm_initialized_ = true;

    esp_lcd_rgb_panel_config_t config{};
    config.clk_src = LCD_CLK_SRC_DEFAULT;
    config.data_width = 16;
    config.bits_per_pixel = 16;
    config.num_fbs = 2;
    config.flags.fb_in_psram = true;
    // Mandatory bounce mode: frame_complete marks a defined ownership
    // boundary, unlike direct DMA's prefetched chained frame descriptors.
    config.bounce_buffer_size_px = RgbFrame::width * 10;
    config.dma_burst_size = 64;
    config.hsync_gpio_num = board::hsync_pin;
    config.vsync_gpio_num = board::vsync_pin;
    config.de_gpio_num = board::data_enable_pin;
    config.pclk_gpio_num = board::pixel_clock_pin;
    config.disp_gpio_num = board::display_enable_pin;
    std::copy(board::rgb_data_pins.begin(), board::rgb_data_pins.end(), config.data_gpio_nums);
    config.timings.pclk_hz = CONFIG_TESLA_HEADUP_RGB_PCLK_HZ;
    config.timings.h_res = RgbFrame::width;
    config.timings.v_res = RgbFrame::height;
    // NHD Rev3A: HSW+HBP=88, VSW+VBP=32, totals 928 x 525.
    config.timings.hsync_pulse_width = 48;
    config.timings.hsync_back_porch = 40;
    config.timings.hsync_front_porch = 40;
    config.timings.vsync_pulse_width = 1;
    config.timings.vsync_back_porch = 31;
    config.timings.vsync_front_porch = 13;
    config.timings.flags.pclk_active_neg = true;
    esp_lcd_panel_handle_t panel = nullptr;
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&config, &panel), tag, "create RGB panel");
    panel_ = panel;
    void* first = nullptr;
    void* second = nullptr;
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(
        panel, 2, &first, &second), tag, "PSRAM framebuffers");
    framebuffers_[0] = static_cast<std::uint16_t*>(first);
    framebuffers_[1] = static_cast<std::uint16_t*>(second);
    ESP_RETURN_ON_FALSE(esp_ptr_external_ram(framebuffers_[0]) &&
                        esp_ptr_external_ram(framebuffers_[1]),
                        ESP_ERR_INVALID_STATE, tag, "framebuffers must be PSRAM");
    // Driver initially allocates black buffers. Initialize explicitly before
    // starting scanout as well; bounce-buffer reads use the CPU cache.
    for (auto* framebuffer : framebuffers_) {
        std::memset(framebuffer, 0, RgbFrame::pixels * 2U);
    }
    esp_lcd_rgb_panel_event_callbacks_t callbacks{};
    callbacks.on_frame_buf_complete = frame_complete;
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_register_event_callbacks(
        panel, &callbacks, &completed_frames_), tag, "frame callback");

#if !CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS
    dashboard_ = static_cast<std::uint16_t*>(heap_caps_calloc(
        surface_.pixels(), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    constexpr auto draw_bytes = surface_.width * draw_lines * 2U;
    draw_buffer_ = heap_caps_malloc(draw_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(dashboard_ && draw_buffer_, ESP_ERR_NO_MEM, tag, "LVGL buffers");
    lv_init();
    lvgl_initialized_ = true;
    lv_tick_set_cb(milliseconds);
    display_ = lv_display_create(surface_.width, surface_.height);
    ESP_RETURN_ON_FALSE(display_, ESP_ERR_NO_MEM, tag, "LVGL display");
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(display_, this);
    lv_display_set_flush_cb(display_, flush);
    lv_display_set_buffers(display_, draw_buffer_, nullptr, draw_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    ESP_RETURN_ON_FALSE(canvas_.initialize(*display_), ESP_ERR_NO_MEM, tag, "dashboard canvas");
#endif
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), tag, "RGB reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), tag, "RGB start");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), tag, "DISP on");
    initialized_ = true;
    dirty_ = true;
    report_us_ = esp_timer_get_time();
    ESP_LOGI(tag, "RGB ready: 800x480, logical %dx%d at (%d,%d), mirrored=%d, pclk=%d",
             surface_.width, surface_.height, origin_.x, origin_.y, mirrored_, CONFIG_TESLA_HEADUP_RGB_PCLK_HZ);
    return ESP_OK;
}

void NewhavenRgbDisplay::flush(lv_display_t* display, const lv_area_t* area, std::uint8_t* pixels) {
    auto* self = static_cast<NewhavenRgbDisplay*>(lv_display_get_user_data(display));
    if (self && self->ready() && area && pixels) {
        if (area->x1 < 0 || area->y1 < 0 || area->x2 < area->x1 || area->y2 < area->y1 ||
            area->x2 >= surface_.width || area->y2 >= surface_.height) {
            self->fail(ESP_ERR_INVALID_ARG);
            lv_display_flush_ready(display);
            return;
        }
        const int w = area->x2 - area->x1 + 1;
        const int h = area->y2 - area->y1 + 1;
        if (w <= 0 || h <= 0 || w > surface_.width || h > surface_.height ||
            !RgbFrame::update({self->dashboard_, surface_.pixels()},
                              area->x1, area->y1, area->x2, area->y2,
                              {pixels, static_cast<std::size_t>(w) * h * 2U}, surface_)) {
            self->fail(ESP_ERR_INVALID_ARG);
        } else if (lv_display_flush_is_last(display)) {
            self->dirty_ = true;
        }
    }
    // The pixels have been copied into the canonical logical image. Hardware
    // presentation happens later, once for a completed LVGL refresh.
    lv_display_flush_ready(display);
}

void NewhavenRgbDisplay::process() noexcept {
    if (!ready()) {
        return;
    }
#if !CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS
    (void)lv_timer_handler();
#endif
    if (!ready()) {
        return;
    }
    const auto now = esp_timer_get_time();
    if (exchange_.pending()) {
        if (exchange_.retire(completed_frames_.load())) {
            ++presented_;
            if (!lit_) {
                const auto result = apply_brightness(brightness_);
                if (result != ESP_OK) {
                    fail(result);
                    return;
                }
                lit_ = true;
            }
        } else if (now - submitted_us_ > handoff_timeout_us) {
            fail(ESP_ERR_TIMEOUT);
            return;
        }
    }
    if (!exchange_.pending() && dirty_) {
        auto* next = framebuffers_[exchange_.writable_index()];
#if CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS
        if (!RgbDiagnostics::render({next, RgbFrame::pixels}, diagnostic_pattern_, origin_, mirrored_)) {
#else
        if (!RgbFrame::compose({dashboard_, surface_.pixels()},
                               {next, RgbFrame::pixels}, origin_, mirrored_, surface_)) {
#endif
            fail(ESP_ERR_INVALID_ARG);
            return;
        }
        const auto result = esp_lcd_panel_draw_bitmap(
            static_cast<esp_lcd_panel_handle_t>(panel_), 0, 0,
            RgbFrame::width, RgbFrame::height, next);
        if (result != ESP_OK) {
            fail(result);
            return;
        }
        exchange_.submitted(completed_frames_.load());
        submitted_us_ = now;
        dirty_ = false;
    }
#if CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST
    if (now - report_us_ >= 1'000'000) {
        const auto completed = completed_frames_.load();
        ESP_LOGI(tag, "RGB self-test: presented=%lu scanout_frames=%lu interval_ms=%lu free_psram=%u",
                 static_cast<unsigned long>(presented_),
                 static_cast<unsigned long>(completed - last_completed_),
                 static_cast<unsigned long>((now - report_us_) / 1000),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        report_us_ = now;
        presented_ = 0;
        last_completed_ = completed;
    }
#endif
}

void NewhavenRgbDisplay::set_projection_mirrored(bool mirrored) noexcept {
    if (mirrored_ != mirrored) {
        mirrored_ = mirrored;
        dirty_ = true;
    }
}

esp_err_t NewhavenRgbDisplay::set_dashboard_position(int x, int y) noexcept {
    const RgbPlacement requested{x, y};
    if (!RgbFrame::valid(requested, surface_)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (origin_.x != x || origin_.y != y) {
        origin_ = requested;
        dirty_ = true;
    }
    return ESP_OK;
}

esp_err_t NewhavenRgbDisplay::apply_brightness(std::uint8_t percent) noexcept {
    if (percent == 100) {
        return ledc_stop(pwm_mode, pwm_channel, 1); // True constant-high 100%.
    }
    ESP_RETURN_ON_ERROR(ledc_set_duty(pwm_mode, pwm_channel,
        (static_cast<std::uint32_t>(percent) * 1023U + 50U) / 100U), tag, "PWM duty");
    return ledc_update_duty(pwm_mode, pwm_channel);
}

esp_err_t NewhavenRgbDisplay::set_brightness_percent(std::uint8_t percent) noexcept {
    if (percent == 0 || percent > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ready() && lit_) {
        const auto result = apply_brightness(percent);
        if (result != ESP_OK) {
            return result;
        }
    }
    brightness_ = percent;
    return ESP_OK;
}

void NewhavenRgbDisplay::fail(esp_err_t error) noexcept {
    failed_ = true;
    if (pwm_initialized_) {
        (void)ledc_stop(pwm_mode, pwm_channel, 0);
    }
    ESP_LOGE(tag, "RGB output disabled: %s", esp_err_to_name(error));
}

void NewhavenRgbDisplay::shutdown() noexcept {
    initialized_ = false;
    if (pwm_initialized_) {
        (void)ledc_stop(pwm_mode, pwm_channel, 0);
        pwm_initialized_ = false;
    }
    // Stop ISR access to completed_frames_ before freeing any owned memory.
    if (panel_) {
        (void)esp_lcd_panel_disp_on_off(static_cast<esp_lcd_panel_handle_t>(panel_), false);
        (void)esp_lcd_panel_del(static_cast<esp_lcd_panel_handle_t>(panel_));
        panel_ = nullptr;
    }
    canvas_.shutdown();
    if (display_) {
        lv_display_delete(display_);
        display_ = nullptr;
    }
    if (lvgl_initialized_) {
        lv_deinit();
        lvgl_initialized_ = false;
    }
    heap_caps_free(draw_buffer_);
    heap_caps_free(dashboard_);
    draw_buffer_ = nullptr;
    dashboard_ = nullptr;
    framebuffers_[0] = framebuffers_[1] = nullptr;
    exchange_ = {};
    completed_frames_.value = 0;
    lit_ = dirty_ = failed_ = false;
}

}  // namespace tesla_headup::platform
