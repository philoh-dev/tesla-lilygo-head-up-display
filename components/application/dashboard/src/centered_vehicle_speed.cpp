#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"

#include <string_view>

namespace tesla_headup::application {
namespace {

// Center the value and unit as one visual block while preserving their spacing.
constexpr std::int16_t value_vertical_offset = -24;
constexpr std::int16_t unit_vertical_offset = 51;

std::uint16_t displayed_speed(
    domain::VehicleSpeed speed,
    domain::UnitSystem units) noexcept {
    const auto kilometres_per_hour = speed.kilometres_per_hour();
    if (units == domain::UnitSystem::Metric) {
        return kilometres_per_hour;
    }

    // 1 km/h = 0.621371 mph. Integer arithmetic keeps presentation
    // deterministic and rounds to the nearest whole mph.
    return static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(kilometres_per_hour) * 621'371U +
         500'000U) /
        1'000'000U);
}

}  // namespace

CenteredVehicleSpeed::CenteredVehicleSpeed() noexcept { format_speed(); }

FeatureId CenteredVehicleSpeed::id() const noexcept {
    return FeatureId::VehicleSpeed;
}

void CenteredVehicleSpeed::render(DashboardCanvas& canvas) const {
    canvas.draw_text(TextElement{
        .feature = FeatureId::VehicleSpeed,
        .text = std::string_view{speed_text_.data()},
        .role = TextRole::PrimaryValue,
        .anchor = Anchor::Center,
        .vertical_offset = value_vertical_offset,
    });
    canvas.draw_text(TextElement{
        .feature = FeatureId::VehicleSpeed,
        .text = units_ == domain::UnitSystem::Metric ? "km/h" : "mph",
        .role = TextRole::Unit,
        .anchor = Anchor::Center,
        .vertical_offset = unit_vertical_offset,
    });
}

void CenteredVehicleSpeed::set_speed(domain::VehicleSpeed speed) noexcept {
    speed_ = speed;
    format_speed();
}

domain::VehicleSpeed CenteredVehicleSpeed::speed() const noexcept {
    return speed_;
}

void CenteredVehicleSpeed::set_unit_system(domain::UnitSystem units) noexcept {
    units_ = units;
    format_speed();
}

domain::UnitSystem CenteredVehicleSpeed::unit_system() const noexcept {
    return units_;
}

void CenteredVehicleSpeed::format_speed() noexcept {
    const std::uint16_t value = displayed_speed(speed_, units_);
    speed_text_.fill('\0');

    if (value >= 100) {
        speed_text_[0] = static_cast<char>('0' + (value / 100));
        speed_text_[1] = static_cast<char>('0' + ((value / 10) % 10));
        speed_text_[2] = static_cast<char>('0' + (value % 10));
        return;
    }

    if (value >= 10) {
        speed_text_[0] = static_cast<char>('0' + (value / 10));
        speed_text_[1] = static_cast<char>('0' + (value % 10));
        return;
    }

    speed_text_[0] = static_cast<char>('0' + value);
}

}  // namespace tesla_headup::application
