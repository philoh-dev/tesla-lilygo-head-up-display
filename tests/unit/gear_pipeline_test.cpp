#include "test.hpp"

#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/selected_gear_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/selected_gear_presenter.hpp"

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
        role = element.role;
    }
    void draw_symbol(const tesla_headup::application::SymbolElement&) override {}
    void draw_edge_gradient(
        const tesla_headup::application::EdgeGradientElement&) override {}
    void end_frame() override {}

    std::vector<std::string> texts;
    tesla_headup::application::TextRole role{};
};

std::array<std::uint8_t, 19> gear_packet(
    std::uint8_t raw,
    std::uint32_t timestamp = 1) {
    return {
        1,
        static_cast<std::uint8_t>(timestamp),
        static_cast<std::uint8_t>(timestamp >> 8U),
        static_cast<std::uint8_t>(timestamp >> 16U),
        static_cast<std::uint8_t>(timestamp >> 24U),
        0,
        0x18, 0x01, 0, 0,
        8,
        0, 0, static_cast<std::uint8_t>(raw << 5U), 0, 0, 0, 0, 0,
    };
}

struct GearRuntime {
    RecordingCanvas canvas;
    tesla_headup::application::TelemetryStore telemetry;
    tesla_headup::application::SelectedGearIndicator feature;
    tesla_headup::application::Dashboard dashboard{canvas};
    tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::SelectedGearPresenter presenter{
        telemetry, feature, dashboard, scheduler};
    tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};

    GearRuntime() { CHECK(dashboard.add_feature(feature, false)); }
};

TEST_CASE("selected gear feature renders every supported label") {
    GearRuntime runtime;
    constexpr std::array gears{
        tesla_headup::domain::SelectedGear::Park,
        tesla_headup::domain::SelectedGear::Reverse,
        tesla_headup::domain::SelectedGear::Neutral,
        tesla_headup::domain::SelectedGear::Drive,
    };
    constexpr std::array labels{"P", "R", "N", "D"};
    for (std::size_t index = 0; index < gears.size(); ++index) {
        runtime.feature.set_gear(gears[index]);
        (void)runtime.dashboard.set_enabled(
            tesla_headup::application::FeatureId::SelectedGear, true);
        runtime.dashboard.render();
        CHECK_EQ(runtime.canvas.texts.size(), 1U);
        CHECK_EQ(runtime.canvas.texts.front(), labels[index]);
    }
}

TEST_CASE("DashKit gear payload reaches the dashboard") {
    GearRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    const auto result = runtime.ingestor.ingest(gear_packet(4), 10ms);
    CHECK_EQ(result.decoded_gear_frames, 1);
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));
    CHECK_EQ(runtime.canvas.texts.size(), 1U);
    CHECK_EQ(runtime.canvas.texts.front(), "D");
    CHECK_EQ(runtime.canvas.role, tesla_headup::application::TextRole::Gear);
}

TEST_CASE("stale and unavailable gear data hides the indicator") {
    GearRuntime runtime;
    CHECK(runtime.scheduler.tick(0ms));
    CHECK_EQ(runtime.ingestor.ingest(gear_packet(2), 10ms).decoded_gear_frames, 1);
    CHECK(runtime.presenter.refresh(10ms));
    CHECK(runtime.scheduler.tick(40ms));
    CHECK_EQ(runtime.canvas.texts.front(), "R");

    CHECK(runtime.presenter.refresh(511ms));
    CHECK(runtime.scheduler.tick(511ms));
    CHECK(runtime.canvas.texts.empty());

    CHECK_EQ(
        runtime.ingestor.ingest(gear_packet(0, 2), 520ms)
            .unavailable_gear_frames,
        1);
    CHECK(!runtime.presenter.refresh(520ms));
    CHECK_EQ(
        runtime.telemetry.selected_gear(520ms).status,
        tesla_headup::application::SignalStatus::Unavailable);
}

TEST_CASE("undefined gear payload is rejected and fails the indicator dark") {
    GearRuntime runtime;
    const auto result = runtime.ingestor.ingest(gear_packet(5), 10ms);
    CHECK_EQ(result.rejected_gear_frames, 1);
    CHECK_EQ(
        runtime.telemetry.selected_gear(10ms).status,
        tesla_headup::application::SignalStatus::Unavailable);
    CHECK(!runtime.presenter.refresh(10ms));
}

}  // namespace
