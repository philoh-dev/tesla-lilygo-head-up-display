#include "tesla_headup/adapters/lvgl_rm67162/prepared_image_storage.hpp"

#include <cstdlib>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace tesla_headup::platform::assets {
bool unpack_image(std::span<const std::uint8_t> input,
                  std::span<std::uint8_t> output) noexcept {
    std::size_t read = 0, written = 0;
    const auto length = [&input, &read](std::size_t base, std::size_t bound,
                                      std::size_t& result) {
        result = base;
        if (base == 15) {
            unsigned extra;
            do {
                if (read == input.size()) return false;
                extra = input[read++];
                if (result > bound || extra > bound - result) return false;
                result += extra;
            } while (extra == 255);
        }
        return result <= bound;
    };
    while (read < input.size()) {
        const auto token = input[read++];
        std::size_t literals;
        if (!length(token >> 4U, output.size() - written, literals) ||
            literals > input.size() - read) return false;
        for (std::size_t i = 0; i < literals; ++i) output[written++] = input[read++];
        if (read == input.size()) return written == output.size();
        if (input.size() - read < 2) return false;
        const std::size_t offset = input[read] | (static_cast<std::size_t>(input[read + 1]) << 8U);
        read += 2;
        if (offset == 0 || offset > written || output.size() - written < 4) return false;
        std::size_t match;
        if (!length(token & 15U, output.size() - written - 4, match)) return false;
        match += 4;
        // Intentionally byte-wise: LZ4 matches may overlap their own output.
        for (std::size_t i = 0; i < match; ++i) {
            output[written] = output[written - offset];
            ++written;
        }
    }
    return written == output.size();
}

bool retain_images(std::span<PreparedImageStorage> images, unsigned& users) noexcept {
    // Called only by the LVGL owner thread. Multiple host displays share storage.
    if (users != 0) { ++users; return true; }
    users = 1;
    for (auto& image : images) {
#ifdef ESP_PLATFORM
        auto* data = static_cast<std::uint8_t*>(heap_caps_malloc(
            image.unpacked_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
        auto* data = static_cast<std::uint8_t*>(std::malloc(image.unpacked_size));
#endif
        image.descriptor->data = data;
        if (data == nullptr || !unpack_image(image.packed, {data, image.unpacked_size})) {
            release_images(images, users);
            return false;
        }
    }
    return true;
}

void release_images(std::span<PreparedImageStorage> images, unsigned& users) noexcept {
    if (users == 0 || --users != 0) return;
    for (auto& image : images) {
        std::free(const_cast<std::uint8_t*>(image.descriptor->data));
        image.descriptor->data = nullptr;
    }
}
}  // namespace tesla_headup::platform::assets
