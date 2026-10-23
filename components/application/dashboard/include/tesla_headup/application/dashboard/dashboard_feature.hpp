#pragma once

#include "tesla_headup/ports/dashboard_canvas.hpp"

namespace tesla_headup::application {

class DashboardFeature {
public:
    virtual ~DashboardFeature() = default;

    [[nodiscard]] virtual FeatureId id() const noexcept = 0;
    virtual void render(DashboardCanvas& canvas) const = 0;
};

}  // namespace tesla_headup::application
