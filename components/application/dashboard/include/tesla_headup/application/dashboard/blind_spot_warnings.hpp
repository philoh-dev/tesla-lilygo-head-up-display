#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"
#include "tesla_headup/domain/blind_spots.hpp"

namespace tesla_headup::application {

class BlindSpotWarnings final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override;
    void render(DashboardCanvas& canvas) const override;

    void set_blind_spots(domain::BlindSpots blind_spots) noexcept;
    [[nodiscard]] domain::BlindSpots blind_spots() const noexcept;

private:
    domain::BlindSpots blind_spots_{domain::BlindSpots::clear()};
};

}  // namespace tesla_headup::application
