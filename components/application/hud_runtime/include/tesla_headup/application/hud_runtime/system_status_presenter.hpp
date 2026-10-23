#pragma once

#include "tesla_headup/application/dashboard/system_status_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

namespace tesla_headup::application {

enum class ConnectivityState : std::uint8_t {
    Disconnected,
    Connecting,
    Reconnecting,
    Ready,
    PairingRequired,
    Failed,
};

struct SystemStatusPolicy {
    std::chrono::milliseconds telemetry_timeout{500};
};

class SystemStatusPresenter final {
public:
    constexpr SystemStatusPresenter(
        SystemStatusIndicator& feature,
        DashboardRenderScheduler& scheduler,
        SystemStatusPolicy policy = {}) noexcept
        : feature_(feature), scheduler_(scheduler), policy_(policy) {}

    [[nodiscard]] bool set_connectivity(
        ConnectivityState state,
        TelemetryTime now) noexcept;
    [[nodiscard]] bool on_telemetry_received(TelemetryTime now) noexcept;
    [[nodiscard]] bool refresh(TelemetryTime now) noexcept;

private:
    [[nodiscard]] SystemStatus desired_status(TelemetryTime now) const noexcept;
    [[nodiscard]] bool apply(TelemetryTime now) noexcept;

    SystemStatusIndicator& feature_;
    DashboardRenderScheduler& scheduler_;
    SystemStatusPolicy policy_;
    ConnectivityState connectivity_{ConnectivityState::Disconnected};
    std::optional<TelemetryTime> last_telemetry_at_{};
};

}  // namespace tesla_headup::application
