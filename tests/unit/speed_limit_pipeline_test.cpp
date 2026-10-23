#include "test.hpp"

#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/speed_limit_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/speed_limit_presenter.hpp"

#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace {

using namespace std::chrono_literals;

class RecordingCanvas final : public tesla_headup::application::DashboardCanvas {
public:
    void begin_frame() override { texts.clear(); }
    void draw_text(const tesla_headup::application::TextElement& element) override {
        texts.emplace_back(element.text);
        last_role = element.role;
    }
    void draw_symbol(const tesla_headup::application::SymbolElement&) override {}
    void draw_edge_gradient(
        const tesla_headup::application::EdgeGradientElement&) override {}
    void end_frame() override {}

    std::vector<std::string> texts;
    tesla_headup::application::TextRole last_role{
        tesla_headup::application::TextRole::PrimaryValue};
};

std::array<std::uint8_t, 19> das_status_packet(std::uint8_t raw_limit) {
    return {
        1,
        1, 0, 0, 0,
        0,
        0x99, 0x03, 0x00, 0x00,
        8,
        0, raw_limit, 0, 0, 0, 0, 0, 0,
    };
}

TEST_CASE("fused speed limit reaches the allocation-free dashboard feature") {
    RecordingCanvas canvas;
    tesla_headup::application::TelemetryStore telemetry;
    tesla_headup::application::SpeedLimitIndicator feature;
    tesla_headup::application::Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature, false));
    tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::SpeedLimitPresenter presenter{
        telemetry, feature, dashboard, scheduler};
    tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};

    const auto result = ingestor.ingest(das_status_packet(10), 10ms);
    CHECK_EQ(result.decoded_speed_limit_frames, 1U);
    CHECK(presenter.refresh(10ms));
    CHECK(scheduler.tick(40ms));
    CHECK_EQ(canvas.texts.size(), 1U);
    CHECK_EQ(canvas.texts.front(), "50");
    CHECK_EQ(canvas.last_role, tesla_headup::application::TextRole::SpeedLimit);
}

TEST_CASE("unavailable and stale speed limits hide the sign") {
    RecordingCanvas canvas;
    tesla_headup::application::TelemetryStore telemetry;
    tesla_headup::application::SpeedLimitIndicator feature;
    tesla_headup::application::Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature, false));
    tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::SpeedLimitPresenter presenter{
        telemetry, feature, dashboard, scheduler};
    tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};

    CHECK_EQ(
        ingestor.ingest(das_status_packet(10), 10ms)
            .decoded_speed_limit_frames,
        1U);
    CHECK(presenter.refresh(10ms));
    CHECK(presenter.refresh(1511ms));
    CHECK(!dashboard.is_enabled(tesla_headup::application::FeatureId::SpeedLimit));

    CHECK_EQ(
        ingestor.ingest(das_status_packet(31), 1520ms)
            .unavailable_speed_limit_frames,
        1U);
    CHECK(!presenter.refresh(1520ms));

    CHECK(ingestor.ingest(das_status_packet(10), 1560ms).accepted_packet());
    CHECK(presenter.refresh(1560ms));
    CHECK(ingestor.ingest(das_status_packet(31), 1600ms).accepted_packet());
    CHECK(presenter.refresh(1600ms));
    CHECK(!dashboard.is_enabled(tesla_headup::application::FeatureId::SpeedLimit));
}

TEST_CASE("speed-limit sign remains stable across 500ms CAN cycles with jitter") {
    RecordingCanvas canvas;
    tesla_headup::application::TelemetryStore telemetry;
    tesla_headup::application::SpeedLimitIndicator feature;
    tesla_headup::application::Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature, false));
    tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::SpeedLimitPresenter presenter{
        telemetry, feature, dashboard, scheduler};
    tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};

    CHECK(ingestor.ingest(das_status_packet(10), 0ms).accepted_packet());
    CHECK(presenter.refresh(0ms));
    CHECK(scheduler.tick(0ms));
    for (const auto arrival : {520ms, 1050ms, 1600ms}) {
        // The former 500 ms expiry hid the sign before each delayed update.
        CHECK(!presenter.refresh(arrival - 1ms));
        CHECK(dashboard.is_enabled(tesla_headup::application::FeatureId::SpeedLimit));
        CHECK(ingestor.ingest(das_status_packet(10), arrival).accepted_packet());
        CHECK(!presenter.refresh(arrival));
        CHECK(!scheduler.dirty());
    }
    // Tolerate a lost cycle, but never retain the sign beyond its deadline.
    CHECK(!presenter.refresh(3100ms));
    CHECK(presenter.refresh(3101ms));
    CHECK(scheduler.tick(3101ms));
    CHECK(canvas.texts.empty());
    CHECK(ingestor.ingest(das_status_packet(12), 3200ms).accepted_packet());
    CHECK(presenter.refresh(3200ms));
    CHECK(scheduler.tick(3200ms));
    CHECK_EQ(canvas.texts.front(), "60");
    CHECK(ingestor.ingest(das_status_packet(0), 3250ms).accepted_packet());
    CHECK(presenter.refresh(3250ms));
    CHECK(scheduler.tick(3250ms));
    CHECK(canvas.texts.empty());
}

TEST_CASE("speed limit domain rejects zero and values above signal range") {
    CHECK(!tesla_headup::domain::SpeedLimit::from_display_value(0).has_value());
    CHECK(!tesla_headup::domain::SpeedLimit::from_display_value(151).has_value());
    CHECK_EQ(
        tesla_headup::domain::SpeedLimit::from_display_value(150)->display_value(),
        150U);
}

}  // namespace
