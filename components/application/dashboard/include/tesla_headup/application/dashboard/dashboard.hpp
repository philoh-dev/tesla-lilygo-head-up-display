#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"

#include <array>
#include <cstddef>

namespace tesla_headup::application {

class Dashboard final {
public:
    static constexpr std::size_t maximum_feature_count = 9;

    explicit Dashboard(DashboardCanvas& canvas) noexcept;

    [[nodiscard]] bool add_feature(
        DashboardFeature& feature,
        bool enabled = true) noexcept;
    [[nodiscard]] bool replace_feature(DashboardFeature& replacement) noexcept;
    [[nodiscard]] bool set_enabled(FeatureId id, bool enabled) noexcept;
    [[nodiscard]] bool is_enabled(FeatureId id) const noexcept;

    void render() const;

private:
    struct FeatureSlot {
        DashboardFeature* feature{nullptr};
        bool enabled{false};
    };

    [[nodiscard]] FeatureSlot* find(FeatureId id) noexcept;
    [[nodiscard]] const FeatureSlot* find(FeatureId id) const noexcept;

    DashboardCanvas& canvas_;
    std::array<FeatureSlot, maximum_feature_count> features_{};
    std::size_t feature_count_{0};
};

}  // namespace tesla_headup::application
