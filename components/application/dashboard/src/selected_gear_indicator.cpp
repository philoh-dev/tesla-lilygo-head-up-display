#include "tesla_headup/application/dashboard/selected_gear_indicator.hpp"

namespace tesla_headup::application {

FeatureId SelectedGearIndicator::id() const noexcept {
    return FeatureId::SelectedGear;
}

void SelectedGearIndicator::render(DashboardCanvas& canvas) const {
    canvas.draw_text(TextElement{
        .feature = FeatureId::SelectedGear,
        .text = label_.data(),
        .role = TextRole::Gear,
    });
}

void SelectedGearIndicator::set_gear(domain::SelectedGear gear) noexcept {
    gear_ = gear;
    label_[0] = domain::selected_gear_label(gear);
}

domain::SelectedGear SelectedGearIndicator::gear() const noexcept {
    return gear_;
}

}  // namespace tesla_headup::application
