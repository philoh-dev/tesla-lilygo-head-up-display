#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace tesla_headup::domain {

// Energy inputs share a centi-kWh unit, independent of CAN encoding.
struct BatteryEnergy {
    std::uint32_t full_centi_kwh{};
    std::uint32_t remaining_centi_kwh{};
};

class BatteryCharge final {
public:
    [[nodiscard]] static constexpr std::optional<BatteryCharge> from_percent(
        unsigned value) noexcept {
        if (value > 100) return std::nullopt;
        return BatteryCharge{static_cast<std::uint8_t>(value)};
    }
    [[nodiscard]] static constexpr std::optional<BatteryCharge> from_energy(
        BatteryEnergy energy, std::uint32_t buffer_centi_kwh) noexcept {
        if (energy.full_centi_kwh <= buffer_centi_kwh) return std::nullopt;
        const auto available = energy.full_centi_kwh - buffer_centi_kwh;
        const auto remaining = energy.remaining_centi_kwh > buffer_centi_kwh
            ? energy.remaining_centi_kwh - buffer_centi_kwh : 0U;
        // Non-negative integer division deliberately floors the usable-energy
        // percentage: 60.5% and 60.99% both display as 60%, not 61%.
        const auto floored = static_cast<std::uint64_t>(remaining) * 100U / available;
        return from_percent(static_cast<unsigned>(std::min<std::uint64_t>(floored, 100U)));
    }
    [[nodiscard]] constexpr std::uint8_t percent() const noexcept { return percent_; }
    friend constexpr bool operator==(const BatteryCharge&, const BatteryCharge&) noexcept = default;
private:
    explicit constexpr BatteryCharge(std::uint8_t value) noexcept : percent_(value) {}
    std::uint8_t percent_{};
};

}  // namespace tesla_headup::domain
