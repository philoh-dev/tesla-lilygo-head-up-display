#pragma once

#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"

#include <cstdint>

namespace tesla_headup::tesla {

enum class ExteriorLightDecodeStatus : std::uint8_t {
    Ignored,
    FrontLighting,
    FrontLightingRequests,
    RearLighting,
    LightingControls,
    InvalidFrame,
};

enum class ExteriorLampValidity : std::uint8_t {
    Valid,
    Fault,
    Unavailable,
};

struct ExteriorLampDecodeValue {
    ExteriorLampValidity validity{ExteriorLampValidity::Unavailable};
    bool active{false};
};

struct ExteriorLightDecodeResult {
    ExteriorLightDecodeStatus status{ExteriorLightDecodeStatus::Ignored};
    ExteriorLampDecodeValue position_lights{};
    ExteriorLampDecodeValue low_beam{};
    ExteriorLampDecodeValue high_beam{};
    ExteriorLampDecodeValue front_fog{};
    ExteriorLampDecodeValue rear_fog{};
    bool adaptive_high_beam_enabled{false};
    std::uint32_t source_timestamp_microseconds{};
};

// Decoder for legacy physical lamp feedback plus the lighting request and
// control signals forwarded by DashKit's Juniper firmware. Presentation
// semantics still require confirmation against target-car captures.
[[nodiscard]] ExteriorLightDecodeResult decode_exterior_lights(
    const dashkit::protocol::CanFrameView& frame) noexcept;

}  // namespace tesla_headup::tesla
