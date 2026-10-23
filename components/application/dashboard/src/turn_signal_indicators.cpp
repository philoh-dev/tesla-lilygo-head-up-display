#include "tesla_headup/application/dashboard/turn_signal_indicators.hpp"

namespace tesla_headup::application {
namespace {

constexpr std::int16_t horizontal_offset = 175;
constexpr std::int16_t vertical_offset = -58;
constexpr std::uint16_t indicator_width = 32;
constexpr std::uint16_t indicator_height = 30;

SymbolElement indicator(
    Symbol symbol,
    std::int16_t offset,
    bool illuminated) {
    return SymbolElement{
        .feature = FeatureId::TurnSignals,
        .symbol = symbol,
        .role = SymbolRole::TurnSignal,
        .anchor = Anchor::Center,
        .horizontal_offset = offset,
        .vertical_offset = vertical_offset,
        .width = indicator_width,
        .height = indicator_height,
        .illuminated = illuminated,
    };
}

}  // namespace

FeatureId TurnSignalIndicators::id() const noexcept {
    return FeatureId::TurnSignals;
}

void TurnSignalIndicators::render(DashboardCanvas& canvas) const {
    if (signals_.left_active()) {
        canvas.draw_symbol(indicator(
            Symbol::LeftArrow, -horizontal_offset, illuminated_));
    }
    if (signals_.right_active()) {
        canvas.draw_symbol(indicator(
            Symbol::RightArrow, horizontal_offset, illuminated_));
    }
}

void TurnSignalIndicators::set_signals(domain::TurnSignals signals) noexcept {
    signals_ = signals;
}

void TurnSignalIndicators::set_illuminated(bool illuminated) noexcept {
    illuminated_ = illuminated;
}

domain::TurnSignals TurnSignalIndicators::signals() const noexcept {
    return signals_;
}

bool TurnSignalIndicators::illuminated() const noexcept {
    return illuminated_;
}

}  // namespace tesla_headup::application
