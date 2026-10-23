#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"
#include "tesla_headup/domain/speed_limit.hpp"

#include <array>

namespace tesla_headup::application {

class SpeedLimitIndicator final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override;
    void render(DashboardCanvas& canvas) const override;

    void set_speed_limit(domain::SpeedLimit speed_limit) noexcept;
    [[nodiscard]] domain::SpeedLimit speed_limit() const noexcept;

private:
    domain::SpeedLimit speed_limit_{
        *domain::SpeedLimit::from_display_value(50)};
    std::array<char, 4> label_{'5', '0', '\0', '\0'};
};

}  // namespace tesla_headup::application
