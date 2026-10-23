#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"
#include "tesla_headup/domain/power_flow.hpp"

#include <optional>

namespace tesla_headup::application {

class PowerIndicator final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override {
        return FeatureId::Power;
    }
    void render(DashboardCanvas& canvas) const override {
        if (flow_) {
            canvas.draw_power({flow_->permille()});
        }
    }
    void set_power(std::optional<domain::PowerFlow> value) noexcept {
        flow_ = value;
    }
    [[nodiscard]] std::optional<domain::PowerFlow> power() const noexcept {
        return flow_;
    }

private:
    std::optional<domain::PowerFlow> flow_{};
};

}  // namespace tesla_headup::application
