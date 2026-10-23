#include "test.hpp"

#include "tesla_headup/application/dashboard/blind_spot_warnings.hpp"

#include <vector>

namespace {

using tesla_headup::application::BlindSpotWarnings;
using tesla_headup::application::DashboardCanvas;
using tesla_headup::application::DisplayEdge;
using tesla_headup::application::EdgeGradientElement;
using tesla_headup::application::FeatureId;
using tesla_headup::application::GradientRole;
using tesla_headup::application::SymbolElement;
using tesla_headup::application::TextElement;
using tesla_headup::domain::BlindSpots;

class RecordingCanvas final : public DashboardCanvas {
public:
    void begin_frame() override {}
    void draw_text(const TextElement&) override {}
    void draw_symbol(const SymbolElement&) override {}
    void draw_edge_gradient(const EdgeGradientElement& element) override {
        gradients.push_back(element);
    }
    void end_frame() override {}

    std::vector<EdgeGradientElement> gradients;
};

TEST_CASE("clear blind spots render no warning gradients") {
    RecordingCanvas canvas;
    BlindSpotWarnings warnings;

    warnings.render(canvas);

    CHECK(canvas.gradients.empty());
    CHECK_EQ(warnings.blind_spots(), BlindSpots::clear());
}

TEST_CASE("blind spot occupancy renders on the matching display edge") {
    RecordingCanvas canvas;
    BlindSpotWarnings warnings;

    warnings.set_blind_spots(BlindSpots::from_occupancy(true, false));
    warnings.render(canvas);
    CHECK_EQ(canvas.gradients.size(), 1U);
    CHECK_EQ(canvas.gradients.front().edge, DisplayEdge::Left);
    CHECK_EQ(canvas.gradients.front().feature, FeatureId::BlindSpotWarnings);
    CHECK_EQ(
        canvas.gradients.front().role,
        GradientRole::BlindSpotWarning);
    CHECK(canvas.gradients.front().width > 0);

    canvas.gradients.clear();
    warnings.set_blind_spots(BlindSpots::from_occupancy(false, true));
    warnings.render(canvas);
    CHECK_EQ(canvas.gradients.size(), 1U);
    CHECK_EQ(canvas.gradients.front().edge, DisplayEdge::Right);
}

TEST_CASE("both occupied blind spots render two independent gradients") {
    RecordingCanvas canvas;
    BlindSpotWarnings warnings;
    warnings.set_blind_spots(BlindSpots::from_occupancy(true, true));

    warnings.render(canvas);

    CHECK_EQ(canvas.gradients.size(), 2U);
    CHECK_EQ(canvas.gradients.at(0).edge, DisplayEdge::Left);
    CHECK_EQ(canvas.gradients.at(1).edge, DisplayEdge::Right);
}

}  // namespace
