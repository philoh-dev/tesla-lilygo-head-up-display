#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"
#include "tesla_headup/domain/exterior_lights.hpp"

namespace tesla_headup::application {

class ExteriorLightIndicators final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override;
    void render(DashboardCanvas& canvas) const override;

    void set_lights(domain::ExteriorLights lights) noexcept;
    [[nodiscard]] domain::ExteriorLights lights() const noexcept;

private:
    domain::ExteriorLights lights_{domain::ExteriorLights::off()};
};

}  // namespace tesla_headup::application
