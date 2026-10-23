#include "test.hpp"

#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"

#include <string>
#include <utility>
#include <vector>

namespace {

using tesla_headup::application::Anchor;
using tesla_headup::application::CenteredVehicleSpeed;
using tesla_headup::application::Dashboard;
using tesla_headup::application::DashboardCanvas;
using tesla_headup::application::DashboardFeature;
using tesla_headup::application::FeatureId;
using tesla_headup::application::TextElement;
using tesla_headup::application::TextRole;
using tesla_headup::domain::VehicleSpeed;
using tesla_headup::domain::UnitSystem;

struct RecordedText {
    FeatureId feature;
    std::string text;
    TextRole role;
    Anchor anchor;
    std::int16_t horizontal_offset;
    std::int16_t vertical_offset;
};

class RecordingCanvas final : public DashboardCanvas {
public:
    void begin_frame() override {
        ++begin_count;
        text.clear();
    }

    void draw_text(const TextElement& element) override {
        text.push_back({
            element.feature,
            std::string{element.text},
            element.role,
            element.anchor,
            element.horizontal_offset,
            element.vertical_offset,
        });
    }

    void draw_symbol(const tesla_headup::application::SymbolElement&) override {}
    void draw_edge_gradient(
        const tesla_headup::application::EdgeGradientElement&) override {}

    void end_frame() override { ++end_count; }

    int begin_count{0};
    int end_count{0};
    std::vector<RecordedText> text;
};

class ReplacementSpeed final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override {
        return FeatureId::VehicleSpeed;
    }

    void render(DashboardCanvas& canvas) const override {
        canvas.draw_text(TextElement{
            .feature = FeatureId::VehicleSpeed,
            .text = "replacement",
            .role = TextRole::PrimaryValue,
            .anchor = Anchor::Center,
        });
    }
};

VehicleSpeed speed(std::uint16_t value) {
    const auto result = VehicleSpeed::from_kilometres_per_hour(value);
    CHECK(result.has_value());
    return *result;
}

TEST_CASE("centered vehicle speed renders metric value in the middle") {
    RecordingCanvas canvas;
    CenteredVehicleSpeed vehicle_speed;
    vehicle_speed.set_speed(speed(123));
    Dashboard dashboard{canvas};

    CHECK(dashboard.add_feature(vehicle_speed));
    dashboard.render();

    CHECK_EQ(canvas.begin_count, 1);
    CHECK_EQ(canvas.end_count, 1);
    CHECK_EQ(canvas.text.size(), 2U);
    CHECK_EQ(canvas.text.at(0).text, "123");
    CHECK_EQ(canvas.text.at(0).role, TextRole::PrimaryValue);
    CHECK_EQ(canvas.text.at(0).anchor, Anchor::Center);
    CHECK_EQ(canvas.text.at(0).horizontal_offset, 0);
    CHECK_EQ(canvas.text.at(0).vertical_offset, -24);
    CHECK_EQ(canvas.text.at(1).text, "km/h");
    CHECK_EQ(canvas.text.at(1).role, TextRole::Unit);
    CHECK_EQ(canvas.text.at(1).vertical_offset, 51);
}

TEST_CASE("vehicle speed formatting has no leading zeroes") {
    RecordingCanvas canvas;
    CenteredVehicleSpeed vehicle_speed;
    Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(vehicle_speed));

    for (const auto& [value, expected] :
         std::vector<std::pair<std::uint16_t, std::string>>{
             {0, "0"}, {7, "7"}, {42, "42"}, {400, "400"}}) {
        vehicle_speed.set_speed(speed(value));
        dashboard.render();
        CHECK_EQ(canvas.text.front().text, expected);
    }
}

TEST_CASE("centered vehicle speed converts metric telemetry for imperial UI") {
    RecordingCanvas canvas;
    CenteredVehicleSpeed vehicle_speed;
    vehicle_speed.set_unit_system(UnitSystem::Imperial);
    vehicle_speed.set_speed(speed(100));
    Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(vehicle_speed));

    dashboard.render();

    CHECK_EQ(vehicle_speed.unit_system(), UnitSystem::Imperial);
    CHECK_EQ(canvas.text.at(0).text, "62");
    CHECK_EQ(canvas.text.at(1).text, "mph");

    vehicle_speed.set_speed(speed(400));
    dashboard.render();
    CHECK_EQ(canvas.text.at(0).text, "249");
}

TEST_CASE("dashboard features can be disabled and enabled") {
    RecordingCanvas canvas;
    CenteredVehicleSpeed vehicle_speed;
    Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(vehicle_speed, false));

    dashboard.render();
    CHECK(canvas.text.empty());
    CHECK(!dashboard.is_enabled(FeatureId::VehicleSpeed));

    CHECK(dashboard.set_enabled(FeatureId::VehicleSpeed, true));
    dashboard.render();
    CHECK_EQ(canvas.text.size(), 2U);
    CHECK(dashboard.is_enabled(FeatureId::VehicleSpeed));
}

TEST_CASE("a feature's visual implementation can be replaced") {
    RecordingCanvas canvas;
    CenteredVehicleSpeed original;
    ReplacementSpeed replacement;
    Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(original));

    CHECK(dashboard.replace_feature(replacement));
    dashboard.render();

    CHECK_EQ(canvas.text.size(), 1U);
    CHECK_EQ(canvas.text.front().text, "replacement");
}

TEST_CASE("dashboard rejects duplicate feature identifiers") {
    RecordingCanvas canvas;
    CenteredVehicleSpeed original;
    ReplacementSpeed duplicate;
    Dashboard dashboard{canvas};

    CHECK(dashboard.add_feature(original));
    CHECK(!dashboard.add_feature(duplicate));
}

}  // namespace
