#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace tesla_headup::platform {

// Shared display-only burn-in mitigation. Dashboard coordinates stay unchanged.
class LvglPixelOrbit final {
public:
    static constexpr std::uint32_t interval_ms = 4U * 60U * 1000U;
    static constexpr std::int32_t maximum_offset = 2;

    LvglPixelOrbit() = default;
    ~LvglPixelOrbit();
    LvglPixelOrbit(const LvglPixelOrbit&) = delete;
    LvglPixelOrbit& operator=(const LvglPixelOrbit&) = delete;

    [[nodiscard]] lv_obj_t* initialize(lv_display_t& display) noexcept;
    void shutdown() noexcept;

private:
    static void advance(lv_timer_t* timer);

    lv_obj_t* content_{nullptr};
    lv_timer_t* timer_{nullptr};
    std::size_t next_position_{0};
};

}  // namespace tesla_headup::platform
