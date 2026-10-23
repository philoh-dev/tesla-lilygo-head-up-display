#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"
#include "tesla_headup/domain/battery_charge.hpp"

namespace tesla_headup::application {
class BatteryChargeIndicator final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override { return FeatureId::BatteryCharge; }
    void render(DashboardCanvas& canvas) const override {
        if (charge_) canvas.draw_battery({charge_->percent()});
    }
    void set_charge(std::optional<domain::BatteryCharge> value) noexcept { charge_ = value; }
    [[nodiscard]] std::optional<domain::BatteryCharge> charge() const noexcept { return charge_; }
private:
    std::optional<domain::BatteryCharge> charge_{};
};
}  // namespace tesla_headup::application
