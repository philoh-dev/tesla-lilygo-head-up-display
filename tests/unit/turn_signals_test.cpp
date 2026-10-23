#include "test.hpp"

#include "tesla_headup/application/dashboard/turn_signal_indicators.hpp"
#include "tesla_headup/domain/turn_signals.hpp"

#include <vector>

namespace {

using tesla_headup::application::DashboardCanvas;
using tesla_headup::application::FeatureId;
using tesla_headup::application::Symbol;
using tesla_headup::application::SymbolElement;
using tesla_headup::application::TextElement;
using tesla_headup::application::TurnSignalIndicators;
using tesla_headup::domain::TurnSignals;

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

TEST_CASE("turn signal lamp inputs map to one domain state") {
    CHECK_EQ(TurnSignals::from_lamps(false, false), TurnSignals::off());
    CHECK_EQ(TurnSignals::from_lamps(true, false), TurnSignals::left());
    CHECK_EQ(TurnSignals::from_lamps(false, true), TurnSignals::right());
    CHECK_EQ(TurnSignals::from_lamps(true, true), TurnSignals::hazard());
}

TEST_CASE("left and right signals render on their respective sides") {
    RecordingCanvas canvas;
    TurnSignalIndicators indicators;

    indicators.set_signals(TurnSignals::left());
    indicators.render(canvas);
    CHECK_EQ(canvas.symbols.size(), 1U);
    CHECK_EQ(canvas.symbols.front().feature, FeatureId::TurnSignals);
    CHECK_EQ(canvas.symbols.front().symbol, Symbol::LeftArrow);
    CHECK(canvas.symbols.front().horizontal_offset < 0);
    CHECK(canvas.symbols.front().visible);

    canvas.symbols.clear();
    indicators.set_signals(TurnSignals::right());
    indicators.render(canvas);
    CHECK_EQ(canvas.symbols.size(), 1U);
    CHECK_EQ(canvas.symbols.front().symbol, Symbol::RightArrow);
    CHECK(canvas.symbols.front().horizontal_offset > 0);
    CHECK(canvas.symbols.front().visible);
}

TEST_CASE("hazard warning renders both turn indicators") {
    RecordingCanvas canvas;
    TurnSignalIndicators indicators;
    indicators.set_signals(TurnSignals::hazard());

    indicators.render(canvas);

    CHECK_EQ(canvas.symbols.size(), 2U);
    CHECK_EQ(canvas.symbols.at(0).symbol, Symbol::LeftArrow);
    CHECK_EQ(canvas.symbols.at(1).symbol, Symbol::RightArrow);
}

TEST_CASE("active turn arrows remain visible through bright and dim phases") {
    for (const auto signals :
         {TurnSignals::left(), TurnSignals::right(), TurnSignals::hazard()}) {
        RecordingCanvas canvas;
        TurnSignalIndicators indicators;
        indicators.set_signals(signals);
        for (const bool illuminated : {false, true, false}) {
            canvas.symbols.clear();
            indicators.set_illuminated(illuminated);
            indicators.render(canvas);
            CHECK_EQ(canvas.symbols.size(),
                     signals == TurnSignals::hazard() ? 2U : 1U);
            for (const auto& symbol : canvas.symbols) {
                CHECK(symbol.visible);
                CHECK_EQ(symbol.illuminated, illuminated);
            }
        }
        canvas.symbols.clear();
        indicators.set_signals(TurnSignals::off());
        indicators.render(canvas);
        CHECK(canvas.symbols.empty());
    }
}

TEST_CASE("off state emits nothing and dark phase preserves signal intent") {
    RecordingCanvas canvas;
    TurnSignalIndicators indicators;

    indicators.render(canvas);
    CHECK(canvas.symbols.empty());

    indicators.set_signals(TurnSignals::hazard());
    indicators.set_illuminated(false);
    indicators.render(canvas);
    CHECK_EQ(canvas.symbols.size(), 2U);
    CHECK(canvas.symbols.at(0).visible);
    CHECK(canvas.symbols.at(1).visible);
    CHECK(!canvas.symbols.at(0).illuminated);
    CHECK(!canvas.symbols.at(1).illuminated);
    CHECK(!indicators.illuminated());
}

}  // namespace
