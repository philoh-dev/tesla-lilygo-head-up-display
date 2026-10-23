#pragma once

#include "tesla_headup/application/dashboard/dashboard.hpp"

#include <chrono>

namespace tesla_headup::application {

using RenderTime = std::chrono::milliseconds;

struct RenderSchedulePolicy {
    std::chrono::milliseconds minimum_frame_interval{40};
};

// Coalesces any number of state changes into one dashboard frame and caps the
// render rate. The first tick always renders the initial dashboard state.
class DashboardRenderScheduler final {
public:
    explicit constexpr DashboardRenderScheduler(
        Dashboard& dashboard,
        RenderSchedulePolicy policy = {}) noexcept
        : dashboard_(dashboard), policy_(policy) {}

    void request_render() noexcept;
    [[nodiscard]] bool tick(RenderTime now);
    [[nodiscard]] constexpr bool dirty() const noexcept { return dirty_; }

private:
    Dashboard& dashboard_;
    RenderSchedulePolicy policy_;
    RenderTime last_render_at_{};
    bool rendered_{false};
    bool dirty_{true};
};

}  // namespace tesla_headup::application
