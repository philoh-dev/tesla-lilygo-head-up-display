#include "tesla_headup/application/dashboard/exterior_light_indicators.hpp"

namespace tesla_headup::application {
namespace {

constexpr std::int16_t vertical_offset = 96;
constexpr std::int16_t slot_spacing = 42;
constexpr std::uint16_t indicator_width = 43;
constexpr std::uint16_t indicator_height = 22;

SymbolElement indicator(Symbol symbol, std::int16_t horizontal_offset) {
    return SymbolElement{
        .feature = FeatureId::ExteriorLights,
        .symbol = symbol,
        .role = SymbolRole::ExteriorLight,
        .anchor = Anchor::Center,
        .horizontal_offset = horizontal_offset,
        .vertical_offset = vertical_offset,
        .width = indicator_width,
        .height = indicator_height,
    };
}

}  // namespace

FeatureId ExteriorLightIndicators::id() const noexcept {
    return FeatureId::ExteriorLights;
}

void ExteriorLightIndicators::render(DashboardCanvas& canvas) const {
    if (lights_.position_lights_active()) {
        canvas.draw_symbol(indicator(Symbol::PositionLights, -2 * slot_spacing));
    }
    if (lights_.low_beam_active()) {
        canvas.draw_symbol(indicator(Symbol::LowBeam, -slot_spacing));
    }
    if (lights_.high_beam_active()) {
        canvas.draw_symbol(indicator(Symbol::HighBeam, 0));
    } else if (lights_.adaptive_high_beam_active()) {
        canvas.draw_symbol(indicator(Symbol::AdaptiveHighBeam, 0));
    }
    if (lights_.front_fog_active()) {
        canvas.draw_symbol(indicator(Symbol::FrontFogLight, slot_spacing));
    }
    if (lights_.rear_fog_active()) {
        canvas.draw_symbol(indicator(Symbol::RearFogLight, 2 * slot_spacing - 10));
    }
}

void ExteriorLightIndicators::set_lights(domain::ExteriorLights lights) noexcept {
    lights_ = lights;
}

domain::ExteriorLights ExteriorLightIndicators::lights() const noexcept {
    return lights_;
}

}  // namespace tesla_headup::application
