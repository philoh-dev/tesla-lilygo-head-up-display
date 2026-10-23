#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"
#include "tesla_headup/domain/selected_gear.hpp"

#include <array>

namespace tesla_headup::application {

class SelectedGearIndicator final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override;
    void render(DashboardCanvas& canvas) const override;

    void set_gear(domain::SelectedGear gear) noexcept;
    [[nodiscard]] domain::SelectedGear gear() const noexcept;

private:
    domain::SelectedGear gear_{domain::SelectedGear::Park};
    std::array<char, 2> label_{'P', '\0'};
};

}  // namespace tesla_headup::application
