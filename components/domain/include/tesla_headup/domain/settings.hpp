#pragma once

#include <cstdint>
#include <optional>

namespace tesla_headup::domain {

enum class UnitSystem : std::uint8_t {
    Metric,
    Imperial,
};

class BrightnessPreference final {
public:
    static constexpr std::uint8_t minimum_percent = 1;
    static constexpr std::uint8_t maximum_percent = 100;

    constexpr BrightnessPreference() noexcept = default;

    [[nodiscard]] static constexpr std::optional<BrightnessPreference>
    from_percent(std::uint8_t percent) noexcept {
        if (percent < minimum_percent || percent > maximum_percent) {
            return std::nullopt;
        }
        return BrightnessPreference{percent};
    }

    [[nodiscard]] constexpr std::uint8_t percent() const noexcept {
        return percent_;
    }

    friend constexpr bool operator==(
        const BrightnessPreference&,
        const BrightnessPreference&) = default;

private:
    explicit constexpr BrightnessPreference(std::uint8_t percent) noexcept
        : percent_(percent) {}

    std::uint8_t percent_{maximum_percent};
};

struct EnabledDashboardFeatures {
    bool vehicle_speed{true};
    bool turn_signals{true};
    bool exterior_lights{true};
    bool autopilot{true};
    bool blind_spot_warnings{true};
    bool selected_gear{true};

    friend constexpr bool operator==(
        const EnabledDashboardFeatures&,
        const EnabledDashboardFeatures&) = default;
};

struct Settings {
    UnitSystem units{UnitSystem::Metric};
    bool projection_mirrored{true};
    BrightnessPreference brightness{};
    EnabledDashboardFeatures enabled_features{};

    friend constexpr bool operator==(
        const Settings&,
        const Settings&) = default;
};

}  // namespace tesla_headup::domain
