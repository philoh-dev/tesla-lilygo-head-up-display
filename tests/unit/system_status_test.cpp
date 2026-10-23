#include "test.hpp"

#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/system_status_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/system_status_presenter.hpp"

#include <array>
#include <chrono>
#include <string>
#include <string_view>

namespace {

using namespace std::chrono_literals;
using tesla_headup::application::ConnectivityState;
using tesla_headup::application::Dashboard;
using tesla_headup::application::DashboardCanvas;
using tesla_headup::application::DashboardRenderScheduler;
using tesla_headup::application::EdgeGradientElement;
using tesla_headup::application::SymbolElement;
using tesla_headup::application::SystemStatus;
using tesla_headup::application::SystemStatusIndicator;
using tesla_headup::application::SystemStatusPresenter;
using tesla_headup::application::TextElement;
using tesla_headup::application::TextRole;

class RecordingCanvas final : public DashboardCanvas {
public:
    void begin_frame() override { text.clear(); }
    void draw_text(const TextElement& element) override {
        text = std::string{element.text};
        role = element.role;
        vertical_offset = element.vertical_offset;
    }
    void draw_symbol(const SymbolElement&) override {}
    void draw_edge_gradient(const EdgeGradientElement&) override {}
    void end_frame() override {}

    std::string text;
    TextRole role{TextRole::StatusInfo};
    std::int16_t vertical_offset{0};
};

struct StatusRuntime {
    RecordingCanvas canvas;
    SystemStatusIndicator feature;
    Dashboard dashboard{canvas};
    DashboardRenderScheduler scheduler{dashboard};
    SystemStatusPresenter presenter{feature, scheduler};

    StatusRuntime() { CHECK(dashboard.add_feature(feature)); }
};

TEST_CASE("system status indicator renders compact severity-coded labels") {
    StatusRuntime runtime;

    struct ExpectedStatus {
        SystemStatus status;
        std::string_view label;
        TextRole role;
    };
    constexpr std::array expected{
        ExpectedStatus{SystemStatus::Disconnected,
                       "DISCONNECTED",
                       TextRole::StatusCritical},
        ExpectedStatus{SystemStatus::Connecting,
                       "CONNECTING",
                       TextRole::StatusInfo},
        ExpectedStatus{SystemStatus::Reconnecting,
                       "RECONNECTING",
                       TextRole::StatusWarning},
        ExpectedStatus{SystemStatus::PairingRequired,
                       "PAIRING REQUIRED",
                       TextRole::StatusCritical},
        ExpectedStatus{SystemStatus::ConnectionFailed,
                       "CONNECTION FAILED",
                       TextRole::StatusCritical},
        ExpectedStatus{SystemStatus::WaitingForData,
                       "WAITING FOR DATA",
                       TextRole::StatusInfo},
        ExpectedStatus{
            SystemStatus::NoData, "NO DATA", TextRole::StatusWarning},
    };
    for (const auto& item : expected) {
        runtime.feature.set_status(item.status);
        runtime.dashboard.render();
        CHECK_EQ(runtime.canvas.text, item.label);
        CHECK_EQ(runtime.canvas.role, item.role);
        CHECK_EQ(runtime.canvas.vertical_offset, -104);
    }

    runtime.feature.set_status(SystemStatus::Hidden);
    runtime.dashboard.render();
    CHECK(runtime.canvas.text.empty());
}

TEST_CASE("ready connection waits for a valid telemetry packet") {
    StatusRuntime runtime;

    CHECK(runtime.presenter.set_connectivity(ConnectivityState::Ready, 10ms));
    CHECK_EQ(runtime.feature.status(), SystemStatus::WaitingForData);
    CHECK(!runtime.presenter.refresh(10s));
    CHECK_EQ(runtime.feature.status(), SystemStatus::WaitingForData);

    CHECK(runtime.presenter.on_telemetry_received(10s));
    CHECK_EQ(runtime.feature.status(), SystemStatus::Hidden);
}

TEST_CASE("telemetry status changes to no data after the freshness window") {
    StatusRuntime runtime;
    CHECK(runtime.presenter.set_connectivity(ConnectivityState::Ready, 0ms));
    CHECK(runtime.presenter.on_telemetry_received(10ms));

    CHECK(!runtime.presenter.refresh(510ms));
    CHECK_EQ(runtime.feature.status(), SystemStatus::Hidden);
    CHECK(runtime.presenter.refresh(511ms));
    CHECK_EQ(runtime.feature.status(), SystemStatus::NoData);

    CHECK(runtime.presenter.on_telemetry_received(520ms));
    CHECK_EQ(runtime.feature.status(), SystemStatus::Hidden);
}

TEST_CASE("late telemetry cannot shorten the current freshness window") {
    StatusRuntime runtime;
    CHECK(runtime.presenter.set_connectivity(ConnectivityState::Ready, 0ms));
    CHECK(runtime.presenter.on_telemetry_received(100ms));
    CHECK(!runtime.presenter.on_telemetry_received(50ms));

    CHECK(!runtime.presenter.refresh(600ms));
    CHECK_EQ(runtime.feature.status(), SystemStatus::Hidden);
    CHECK(runtime.presenter.refresh(601ms));
    CHECK_EQ(runtime.feature.status(), SystemStatus::NoData);
}

TEST_CASE("connection failures override telemetry freshness immediately") {
    StatusRuntime runtime;
    CHECK(runtime.presenter.set_connectivity(ConnectivityState::Ready, 0ms));
    CHECK(runtime.presenter.on_telemetry_received(1ms));

    CHECK(runtime.presenter.set_connectivity(
        ConnectivityState::Reconnecting, 2ms));
    CHECK_EQ(runtime.feature.status(), SystemStatus::Reconnecting);
    CHECK(!runtime.presenter.on_telemetry_received(3ms));

    CHECK(runtime.presenter.set_connectivity(
        ConnectivityState::PairingRequired, 4ms));
    CHECK_EQ(runtime.feature.status(), SystemStatus::PairingRequired);
    CHECK(runtime.presenter.set_connectivity(ConnectivityState::Failed, 5ms));
    CHECK_EQ(runtime.feature.status(), SystemStatus::ConnectionFailed);
}

TEST_CASE("negative telemetry timeout expires on the next clock tick") {
    RecordingCanvas canvas;
    SystemStatusIndicator feature;
    Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature));
    DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::SystemStatusPolicy policy;
    policy.telemetry_timeout = -1ms;
    SystemStatusPresenter presenter{feature, scheduler, policy};
    CHECK(presenter.set_connectivity(ConnectivityState::Ready, 0ms));
    CHECK(presenter.on_telemetry_received(10ms));

    CHECK(!presenter.refresh(10ms));
    CHECK(presenter.refresh(11ms));
    CHECK_EQ(feature.status(), SystemStatus::NoData);
}

}  // namespace
