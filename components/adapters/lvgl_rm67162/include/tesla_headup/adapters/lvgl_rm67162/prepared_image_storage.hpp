#pragma once

#include "lvgl.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace tesla_headup::platform::assets {
struct PreparedImageStorage {
    lv_image_dsc_t* descriptor;
    std::span<const std::uint8_t> packed;
    std::size_t unpacked_size;
};
// Bounded lossless block decoder, used only while initializing native assets.
[[nodiscard]] bool unpack_image(std::span<const std::uint8_t> input,
                                std::span<std::uint8_t> output) noexcept;
[[nodiscard]] bool retain_images(std::span<PreparedImageStorage> images, unsigned& users) noexcept;
void release_images(std::span<PreparedImageStorage> images, unsigned& users) noexcept;
}  // namespace tesla_headup::platform::assets
