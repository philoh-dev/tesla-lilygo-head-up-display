#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"

namespace tesla_headup::application {

void DashboardRenderScheduler::request_render() noexcept { dirty_ = true; }

bool DashboardRenderScheduler::tick(RenderTime now) {
    if (!dirty_) {
        return false;
    }

    const auto interval = policy_.minimum_frame_interval < RenderTime::zero()
                              ? RenderTime::zero()
                              : policy_.minimum_frame_interval;
    const bool clock_moved_backwards = rendered_ && now < last_render_at_;
    const bool interval_elapsed =
        !rendered_ || clock_moved_backwards ||
        (now - last_render_at_) >= interval;
    if (!interval_elapsed) {
        return false;
    }

    dashboard_.render();
    last_render_at_ = now;
    rendered_ = true;
    dirty_ = false;
    return true;
}

}  // namespace tesla_headup::application
