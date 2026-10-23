#pragma once

#include <cstdint>
#include <optional>

namespace tesla_headup::domain {

class SpeedLimit final {
public:
    static constexpr std::uint16_t maximum_display_value = 150;

    [[nodiscard]] static constexpr std::optional<SpeedLimit> from_display_value(
        std::uint16_t value) noexcept {
        if (value == 0 || value > maximum_display_value) {
            return std::nullopt;
        }
        return SpeedLimit{value};
    }

    [[nodiscard]] constexpr std::uint16_t display_value() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(
        const SpeedLimit&,
        const SpeedLimit&) noexcept = default;

private:
    explicit constexpr SpeedLimit(std::uint16_t value) noexcept
        : value_(value) {}

    std::uint16_t value_{};
};

}  // namespace tesla_headup::domain
