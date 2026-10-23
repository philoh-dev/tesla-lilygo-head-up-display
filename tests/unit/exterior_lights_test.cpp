#include "test.hpp"

#include "tesla_headup/application/dashboard/exterior_light_indicators.hpp"
#include "tesla_headup/domain/exterior_lights.hpp"

#include <vector>

namespace {

using tesla_headup::application::DashboardCanvas;
using tesla_headup::application::ExteriorLightIndicators;
using tesla_headup::application::FeatureId;
using tesla_headup::application::Symbol;
using tesla_headup::application::SymbolElement;
using tesla_headup::application::SymbolRole;
using tesla_headup::application::TextElement;
using tesla_headup::domain::ExteriorLights;
using tesla_headup::domain::HighBeamMode;

class RecordingCanvas final : public DashboardCanvas {
public:
    void begin_frame() override {}
    void draw_text(const TextElement&) override {}
    void draw_symbol(const SymbolElement& element) override {
        symbols.push_back(element);
    }
    void draw_edge_gradient(
        const tesla_headup::application::EdgeGradientElement&) override {}
    void end_frame() override {}

    std::vector<SymbolElement> symbols;
};

TEST_CASE("exterior lights preserve CAN lamp state with exclusive high beams") {
    const auto lights = ExteriorLights::from_lamps(
        true, false, HighBeamMode::Adaptive, true, false);

    CHECK(lights.position_lights_active());
    CHECK(!lights.low_beam_active());
    CHECK(!lights.high_beam_active());
    CHECK(lights.adaptive_high_beam_active());
    CHECK_EQ(lights.high_beam_mode(), HighBeamMode::Adaptive);
    CHECK(lights.front_fog_active());
    CHECK(!lights.rear_fog_active());
}

TEST_CASE("inactive exterior lights render no symbols") {
    RecordingCanvas canvas;
    ExteriorLightIndicators indicators;

    indicators.render(canvas);

    CHECK(canvas.symbols.empty());
    CHECK_EQ(indicators.lights(), ExteriorLights::off());
}

TEST_CASE("active exterior lights render in stable dedicated slots") {
    RecordingCanvas canvas;
    ExteriorLightIndicators indicators;
    indicators.set_lights(
        ExteriorLights::from_lamps(
            true, true, HighBeamMode::Manual, true, true));

    indicators.render(canvas);

    CHECK_EQ(canvas.symbols.size(), 5U);
    CHECK_EQ(canvas.symbols.at(0).symbol, Symbol::PositionLights);
    CHECK_EQ(canvas.symbols.at(1).symbol, Symbol::LowBeam);
    CHECK_EQ(canvas.symbols.at(2).symbol, Symbol::HighBeam);
    CHECK_EQ(canvas.symbols.at(3).symbol, Symbol::FrontFogLight);
    CHECK_EQ(canvas.symbols.at(4).symbol, Symbol::RearFogLight);
    for (const auto& symbol : canvas.symbols) {
        CHECK_EQ(symbol.feature, FeatureId::ExteriorLights);
        CHECK_EQ(symbol.role, SymbolRole::ExteriorLight);
        CHECK(symbol.vertical_offset > 0);
    }
    CHECK(canvas.symbols.at(0).horizontal_offset <
          canvas.symbols.at(1).horizontal_offset);
    CHECK(canvas.symbols.at(1).horizontal_offset <
          canvas.symbols.at(2).horizontal_offset);
    CHECK(canvas.symbols.at(2).horizontal_offset <
          canvas.symbols.at(3).horizontal_offset);
    CHECK(canvas.symbols.at(3).horizontal_offset <
          canvas.symbols.at(4).horizontal_offset);
}

TEST_CASE("adaptive high beam replaces the manual high-beam symbol") {
    RecordingCanvas canvas;
    ExteriorLightIndicators indicators;
    indicators.set_lights(ExteriorLights::from_lamps(
        false, false, HighBeamMode::Adaptive, false, false));

    indicators.render(canvas);

    CHECK_EQ(canvas.symbols.size(), 1U);
    CHECK_EQ(canvas.symbols.front().symbol, Symbol::AdaptiveHighBeam);
    CHECK_EQ(canvas.symbols.front().horizontal_offset, 0);
}

}  // namespace
