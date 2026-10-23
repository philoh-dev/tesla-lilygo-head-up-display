#pragma once

#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"
#include "tesla_headup/domain/vehicle_speed.hpp"

#include <cstdint>
#include <optional>

namespace tesla_headup::tesla {

enum class SpeedDecodeStatus : std::uint8_t {
    Ignored,
    Decoded,
    Unavailable,
    InvalidFrame,
    OutOfRange,
};

struct SpeedDecodeResult {
    SpeedDecodeStatus status{SpeedDecodeStatus::Ignored};
    std::optional<domain::VehicleSpeed> speed{};
    std::uint32_t source_timestamp_microseconds{};

    [[nodiscard]] constexpr bool decoded() const noexcept {
        return status == SpeedDecodeStatus::Decoded;
    }
};

// Decodes DI_uiSpeed from DI_speed (CAN address 599) on DashKit bus 0.
// Source profile: a Tesla configured for km/h, matching DashPilot's UI source.
// An mph-configured vehicle requires an explicit source-profile change; the
// dashboard's presentation-unit preference does not change this CAN contract.
[[nodiscard]] SpeedDecodeResult decode_vehicle_speed(
    const dashkit::protocol::CanFrameView& frame) noexcept;

}  // namespace tesla_headup::tesla
