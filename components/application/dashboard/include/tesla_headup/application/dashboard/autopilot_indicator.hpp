#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"

namespace tesla_headup::application {

class AutopilotIndicator final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override;
    void render(DashboardCanvas& canvas) const override;

    void set_illuminated(bool illuminated) noexcept;

    [[nodiscard]] bool illuminated() const noexcept;

private:
    bool illuminated_{false};
};

}  // namespace tesla_headup::application
