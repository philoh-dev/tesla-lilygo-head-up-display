#include "tesla_headup/adapters/tesla_decoder/tesla_exterior_lights_decoder.hpp"

#include <cstddef>
#include <cstdint>

namespace tesla_headup::tesla {
namespace {

constexpr std::uint8_t vehicle_bus = 1;
constexpr std::uint32_t body_controls_address = 1001;
constexpr std::uint32_t front_lighting_address = 1009;
constexpr std::uint32_t right_lighting_address = 995;
constexpr std::uint32_t vehicle_control_address = 627;
constexpr std::size_t body_controls_payload_size = 8;
constexpr std::size_t front_lighting_payload_size = 8;
constexpr std::size_t right_lighting_payload_size = 2;
constexpr std::size_t vehicle_control_payload_size = 8;

enum class RawLampStatus : std::uint8_t {
    Off = 0,
    On = 1,
    Fault = 2,
    Unavailable = 3,
};

RawLampStatus lamp_status(std::uint8_t byte, std::uint8_t shift) noexcept {
    return static_cast<RawLampStatus>((byte >> shift) & 0x03U);
}

ExteriorLampDecodeValue decode_lamp(RawLampStatus status) noexcept {
    switch (status) {
    case RawLampStatus::Off:
        return {ExteriorLampValidity::Valid, false};
    case RawLampStatus::On:
        return {ExteriorLampValidity::Valid, true};
    case RawLampStatus::Fault:
        return {ExteriorLampValidity::Fault, false};
    case RawLampStatus::Unavailable:
        return {ExteriorLampValidity::Unavailable, false};
    }
    return {ExteriorLampValidity::Unavailable, false};
}

ExteriorLampDecodeValue decode_headlight_request(
    std::uint8_t request,
    bool include_tail_lights) noexcept {
    // DAS_headlightRequest: OFF=0, ON=1, TAIL_LIGHTS_ONLY=2, INVALID=3.
    if (request == 3U) {
        return {ExteriorLampValidity::Unavailable, false};
    }
    return {
        ExteriorLampValidity::Valid,
        request == 1U || (include_tail_lights && request == 2U),
    };
}

ExteriorLampDecodeValue decode_high_low_beam_decision(
    std::uint8_t decision) noexcept {
    // DAS_highLowBeamDecision: UNDECIDED=0, OFF=1, ON=2, SNA=3.
    if (decision == 3U) {
        return {ExteriorLampValidity::Unavailable, false};
    }
    return {ExteriorLampValidity::Valid, decision == 2U};
}

ExteriorLampDecodeValue decode_lamp_pair(
    RawLampStatus left,
    RawLampStatus right) noexcept {
    if (left == RawLampStatus::Fault || right == RawLampStatus::Fault) {
        return {ExteriorLampValidity::Fault, false};
    }
    if (left == RawLampStatus::Unavailable ||
        right == RawLampStatus::Unavailable) {
        return {ExteriorLampValidity::Unavailable, false};
    }
    return {
        ExteriorLampValidity::Valid,
        left == RawLampStatus::On || right == RawLampStatus::On,
    };
}

ExteriorLightDecodeResult decode_front_lighting(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    if (frame.data.size() != front_lighting_payload_size) {
        return {
            .status = ExteriorLightDecodeStatus::InvalidFrame,
            .source_timestamp_microseconds = frame.timestamp_microseconds,
        };
    }

    const auto low_beam = decode_lamp_pair(
        lamp_status(frame.data[3], 4),
        lamp_status(frame.data[3], 6));
    const auto high_beam = decode_lamp_pair(
        lamp_status(frame.data[4], 0),
        lamp_status(frame.data[4], 2));
    const auto front_fog = decode_lamp_pair(
        lamp_status(frame.data[5], 0),
        lamp_status(frame.data[5], 2));
    const auto position_lights = decode_lamp_pair(
        lamp_status(frame.data[6], 6),
        lamp_status(frame.data[7], 0));

    return {
        .status = ExteriorLightDecodeStatus::FrontLighting,
        .position_lights = position_lights,
        .low_beam = low_beam,
        .high_beam = high_beam,
        .front_fog = front_fog,
        .source_timestamp_microseconds = frame.timestamp_microseconds,
    };
}

ExteriorLightDecodeResult decode_body_controls_lighting(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    if (frame.data.size() != body_controls_payload_size) {
        return {
            .status = ExteriorLightDecodeStatus::InvalidFrame,
            .source_timestamp_microseconds = frame.timestamp_microseconds,
        };
    }

    const auto headlight_request = frame.data[0] & 0x03U;
    const auto high_low_beam_decision = (frame.data[1] >> 3U) & 0x03U;
    return {
        .status = ExteriorLightDecodeStatus::FrontLightingRequests,
        .position_lights =
            decode_headlight_request(headlight_request, true),
        .low_beam = decode_headlight_request(headlight_request, false),
        .high_beam =
            decode_high_low_beam_decision(high_low_beam_decision),
        // Juniper does not expose VCFRONT_lighting through DashKit's hardware
        // allowlist. Front fog is supplied independently by UI_vehicleControl.
        .front_fog = {ExteriorLampValidity::Unavailable, false},
        .source_timestamp_microseconds = frame.timestamp_microseconds,
    };
}

ExteriorLightDecodeResult decode_rear_lighting(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    // DashKit can expose a short classic-CAN message in an eight-byte storage
    // frame. The rear-fog signal is wholly contained in the DBC-defined
    // two-byte prefix, so accept trailing padding but never truncation.
    if (frame.data.size() < right_lighting_payload_size) {
        return {
            .status = ExteriorLightDecodeStatus::InvalidFrame,
            .source_timestamp_microseconds = frame.timestamp_microseconds,
        };
    }

    return {
        .status = ExteriorLightDecodeStatus::RearLighting,
        .rear_fog = decode_lamp(lamp_status(frame.data[1], 0)),
        .source_timestamp_microseconds = frame.timestamp_microseconds,
    };
}

ExteriorLightDecodeResult decode_lighting_controls(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    if (frame.data.size() != vehicle_control_payload_size) {
        return {
            .status = ExteriorLightDecodeStatus::InvalidFrame,
            .source_timestamp_microseconds = frame.timestamp_microseconds,
        };
    }

    return {
        .status = ExteriorLightDecodeStatus::LightingControls,
        .front_fog = {
            ExteriorLampValidity::Valid,
            (frame.data[0] & 0x08U) != 0U,
        },
        .adaptive_high_beam_enabled = (frame.data[5] & 0x02U) != 0U,
        .source_timestamp_microseconds = frame.timestamp_microseconds,
    };
}

}  // namespace

ExteriorLightDecodeResult decode_exterior_lights(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    if (frame.bus != vehicle_bus) {
        return {
            .status = ExteriorLightDecodeStatus::Ignored,
            .source_timestamp_microseconds = frame.timestamp_microseconds,
        };
    }
    if (frame.address == body_controls_address) {
        return decode_body_controls_lighting(frame);
    }
    if (frame.address == front_lighting_address) {
        return decode_front_lighting(frame);
    }
    if (frame.address == right_lighting_address) {
        return decode_rear_lighting(frame);
    }
    if (frame.address == vehicle_control_address) {
        return decode_lighting_controls(frame);
    }
    return {
        .status = ExteriorLightDecodeStatus::Ignored,
        .source_timestamp_microseconds = frame.timestamp_microseconds,
    };
}

}  // namespace tesla_headup::tesla
