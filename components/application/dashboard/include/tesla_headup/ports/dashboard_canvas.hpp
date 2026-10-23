#pragma once

#include <cstdint>
#include <string_view>

namespace tesla_headup::application {

enum class FeatureId : std::uint8_t {
    VehicleSpeed,
    TurnSignals,
    ExteriorLights,
    Autopilot,
    BlindSpotWarnings,
    SystemStatus,
    SelectedGear,
    SpeedLimit,
    Power,
    BatteryCharge,
};

enum class TextRole : std::uint8_t {
    PrimaryValue,
    Unit,
    StatusInfo,
    StatusWarning,
    StatusCritical,
    Gear,
    SpeedLimit,
};

enum class Anchor : std::uint8_t {
    Center,
};

enum class Symbol : std::uint8_t {
    LeftArrow,
    RightArrow,
    PositionLights,
    LowBeam,
    HighBeam,
    AdaptiveHighBeam,
    FrontFogLight,
    RearFogLight,
    Autopilot,
};

enum class SymbolRole : std::uint8_t {
    TurnSignal,
    ExteriorLight,
    Autopilot,
};

enum class DisplayEdge : std::uint8_t {
    Left,
    Right,
};

enum class GradientRole : std::uint8_t {
    BlindSpotWarning,
};

struct TextElement {
    FeatureId feature{FeatureId::VehicleSpeed};
    std::string_view text;
    TextRole role{TextRole::PrimaryValue};
    Anchor anchor{Anchor::Center};
    std::int16_t horizontal_offset{0};
    std::int16_t vertical_offset{0};
};

struct SymbolElement {
    FeatureId feature{FeatureId::TurnSignals};
    Symbol symbol{Symbol::LeftArrow};
    SymbolRole role{SymbolRole::TurnSignal};
    Anchor anchor{Anchor::Center};
    std::int16_t horizontal_offset{0};
    std::int16_t vertical_offset{0};
    std::uint16_t width{0};
    std::uint16_t height{0};
    bool visible{true};
    bool illuminated{true};
};

struct EdgeGradientElement {
    FeatureId feature{FeatureId::BlindSpotWarnings};
    DisplayEdge edge{DisplayEdge::Left};
    GradientRole role{GradientRole::BlindSpotWarning};
    std::uint16_t width{0};
};

struct PowerBarElement {
    // Positive discharge fills upward; negative recuperation fills downward.
    std::int16_t permille{};
};

struct BatteryChargeElement {
    std::uint8_t percent{};
};

class DashboardCanvas {
public:
    virtual ~DashboardCanvas() = default;

    virtual void begin_frame() = 0;
    virtual void draw_text(const TextElement& element) = 0;
    virtual void draw_symbol(const SymbolElement& element) = 0;
    virtual void draw_edge_gradient(const EdgeGradientElement& element) = 0;
    // Layouts without a power rail may omit this presentation primitive.
    virtual void draw_power(const PowerBarElement&) {}
    // Compact/minimal layouts deliberately omit the native battery widget.
    virtual void draw_battery(const BatteryChargeElement&) {}
    virtual void end_frame() = 0;
};

}  // namespace tesla_headup::application
