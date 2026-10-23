#include "tesla_headup/application/dashboard/speed_limit_indicator.hpp"

#include <charconv>

namespace tesla_headup::application {

FeatureId SpeedLimitIndicator::id() const noexcept {
    return FeatureId::SpeedLimit;
}

void SpeedLimitIndicator::render(DashboardCanvas& canvas) const {
    canvas.draw_text(TextElement{
        .feature = FeatureId::SpeedLimit,
        .text = label_.data(),
        .role = TextRole::SpeedLimit,
        .horizontal_offset = 220,
        .vertical_offset = 45,
    });
}

void SpeedLimitIndicator::set_speed_limit(
    domain::SpeedLimit speed_limit) noexcept {
    speed_limit_ = speed_limit;
    label_.fill('\0');
    (void)std::to_chars(
        label_.data(),
        label_.data() + label_.size() - 1,
        speed_limit.display_value());
}

domain::SpeedLimit SpeedLimitIndicator::speed_limit() const noexcept {
    return speed_limit_;
}

}  // namespace tesla_headup::application
