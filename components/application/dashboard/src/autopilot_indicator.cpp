#include "tesla_headup/application/dashboard/autopilot_indicator.hpp"

namespace tesla_headup::application {
namespace {
constexpr std::uint16_t indicator_width = 32;
constexpr std::uint16_t indicator_height = 30;
}  // namespace

FeatureId AutopilotIndicator::id() const noexcept {
    return FeatureId::Autopilot;
}

void AutopilotIndicator::render(DashboardCanvas& canvas) const {
    if (!illuminated_) {
        return;
    }

    canvas.draw_symbol(SymbolElement{
        .feature = FeatureId::Autopilot,
        .symbol = Symbol::Autopilot,
        .role = SymbolRole::Autopilot,
        .horizontal_offset = -175,
        .vertical_offset = 96,
        .width = indicator_width,
        .height = indicator_height,
    });
}

void AutopilotIndicator::set_illuminated(bool illuminated) noexcept {
    illuminated_ = illuminated;
}

bool AutopilotIndicator::illuminated() const noexcept {
    return illuminated_;
}

}  // namespace tesla_headup::application