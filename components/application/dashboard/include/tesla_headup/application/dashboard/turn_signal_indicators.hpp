#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"
#include "tesla_headup/domain/turn_signals.hpp"

namespace tesla_headup::application {

class TurnSignalIndicators final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override;
    void render(DashboardCanvas& canvas) const override;

    void set_signals(domain::TurnSignals signals) noexcept;
    void set_illuminated(bool illuminated) noexcept;

    [[nodiscard]] domain::TurnSignals signals() const noexcept;
    [[nodiscard]] bool illuminated() const noexcept;

private:
    domain::TurnSignals signals_{domain::TurnSignals::off()};
    bool illuminated_{true};
};

}  // namespace tesla_headup::application
