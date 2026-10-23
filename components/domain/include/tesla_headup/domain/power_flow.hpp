#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace tesla_headup::domain {

struct BatteryElectricalState {
    std::uint16_t voltage_centivolts{};
    std::int16_t current_deciamps{};  // Positive discharge, negative charging.
    friend constexpr bool operator==(
        const BatteryElectricalState&, const BatteryElectricalState&) = default;
};

struct BatteryPowerLimits {
    std::uint16_t discharge_centikw{};
    std::uint16_t regeneration_centikw{};
    friend constexpr bool operator==(
        const BatteryPowerLimits&, const BatteryPowerLimits&) = default;
};

// Signed utilization of the applicable battery power limit: -1000..1000.
class PowerFlow final {
public:
    [[nodiscard]] static constexpr std::optional<PowerFlow> from_battery(
        BatteryElectricalState battery, BatteryPowerLimits limits) noexcept {
        if (battery.voltage_centivolts == 0) {
            return std::nullopt;
        }
        const auto limit = battery.current_deciamps < 0
                               ? limits.regeneration_centikw
                               : limits.discharge_centikw;
        if (limit == 0) {
            return std::nullopt;
        }
        // cV * dA is milliwatts; one centi-kW is 10,000 milliwatts.
        const auto scaled =
            (static_cast<std::int64_t>(battery.voltage_centivolts) *
             battery.current_deciamps) / (static_cast<std::int64_t>(limit) * 10);
        return PowerFlow{static_cast<std::int16_t>(
            std::clamp<std::int64_t>(scaled, -1000, 1000))};
    }

    [[nodiscard]] constexpr std::int16_t permille() const noexcept {
        return permille_;
    }
    friend constexpr bool operator==(const PowerFlow&, const PowerFlow&) = default;

private:
    explicit constexpr PowerFlow(std::int16_t value) noexcept : permille_(value) {}
    std::int16_t permille_{};
};

}  // namespace tesla_headup::domain
