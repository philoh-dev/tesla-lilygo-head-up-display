#pragma once

#include <cstdint>
#include <optional>

namespace tesla_headup::domain {

class VehicleSpeed final {
public:
    static constexpr std::uint16_t maximum_kilometres_per_hour = 400;

    [[nodiscard]] static constexpr std::optional<VehicleSpeed>
    from_kilometres_per_hour(std::uint16_t value) noexcept {
        if (value > maximum_kilometres_per_hour) {
            return std::nullopt;
        }
        return VehicleSpeed{value};
    }

    [[nodiscard]] static constexpr VehicleSpeed zero() noexcept {
        return VehicleSpeed{0};
    }

    [[nodiscard]] constexpr std::uint16_t kilometres_per_hour() const noexcept {
        return kilometres_per_hour_;
    }

    friend constexpr bool operator==(const VehicleSpeed&, const VehicleSpeed&) =
        default;

private:
    explicit constexpr VehicleSpeed(std::uint16_t value) noexcept
        : kilometres_per_hour_(value) {}

    std::uint16_t kilometres_per_hour_;
};

}  // namespace tesla_headup::domain
