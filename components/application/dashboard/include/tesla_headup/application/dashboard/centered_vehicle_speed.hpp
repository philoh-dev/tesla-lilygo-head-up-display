#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"
#include "tesla_headup/domain/settings.hpp"
#include "tesla_headup/domain/vehicle_speed.hpp"

#include <array>

namespace tesla_headup::application {

class CenteredVehicleSpeed final : public DashboardFeature {
public:
    CenteredVehicleSpeed() noexcept;

    [[nodiscard]] FeatureId id() const noexcept override;
    void render(DashboardCanvas& canvas) const override;

    void set_speed(domain::VehicleSpeed speed) noexcept;
    [[nodiscard]] domain::VehicleSpeed speed() const noexcept;
    void set_unit_system(domain::UnitSystem units) noexcept;
    [[nodiscard]] domain::UnitSystem unit_system() const noexcept;

private:
    void format_speed() noexcept;

    domain::VehicleSpeed speed_{domain::VehicleSpeed::zero()};
    domain::UnitSystem units_{domain::UnitSystem::Metric};
    std::array<char, 4> speed_text_{{'0', '\0', '\0', '\0'}};
};

}  // namespace tesla_headup::application
