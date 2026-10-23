#include "tesla_headup/application/dashboard/dashboard.hpp"

namespace tesla_headup::application {

Dashboard::Dashboard(DashboardCanvas& canvas) noexcept : canvas_(canvas) {}

bool Dashboard::add_feature(
    DashboardFeature& feature,
    bool enabled) noexcept {
    if (feature_count_ >= features_.size() || find(feature.id()) != nullptr) {
        return false;
    }

    features_[feature_count_] = FeatureSlot{&feature, enabled};
    ++feature_count_;
    return true;
}

bool Dashboard::replace_feature(DashboardFeature& replacement) noexcept {
    FeatureSlot* slot = find(replacement.id());
    if (slot == nullptr) {
        return false;
    }

    slot->feature = &replacement;
    return true;
}

bool Dashboard::set_enabled(FeatureId id, bool enabled) noexcept {
    FeatureSlot* slot = find(id);
    if (slot == nullptr) {
        return false;
    }

    slot->enabled = enabled;
    return true;
}

bool Dashboard::is_enabled(FeatureId id) const noexcept {
    const FeatureSlot* slot = find(id);
    return slot != nullptr && slot->enabled;
}

void Dashboard::render() const {
    canvas_.begin_frame();
    for (std::size_t index = 0; index < feature_count_; ++index) {
        const FeatureSlot& slot = features_[index];
        if (slot.enabled && slot.feature != nullptr) {
            slot.feature->render(canvas_);
        }
    }
    canvas_.end_frame();
}

Dashboard::FeatureSlot* Dashboard::find(FeatureId id) noexcept {
    for (std::size_t index = 0; index < feature_count_; ++index) {
        if (features_[index].feature != nullptr &&
            features_[index].feature->id() == id) {
            return &features_[index];
        }
    }
    return nullptr;
}

const Dashboard::FeatureSlot* Dashboard::find(FeatureId id) const noexcept {
    for (std::size_t index = 0; index < feature_count_; ++index) {
        if (features_[index].feature != nullptr &&
            features_[index].feature->id() == id) {
            return &features_[index];
        }
    }
    return nullptr;
}

}  // namespace tesla_headup::application
