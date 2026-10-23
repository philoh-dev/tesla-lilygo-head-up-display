#pragma once

#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"
#include "tesla_headup/domain/autopilot_state.hpp"
#include "tesla_headup/domain/speed_limit.hpp"

#include <cstdint>
#include <optional>

namespace tesla_headup::tesla {

enum class DriverAssistanceDecodeStatus : std::uint8_t {
    Ignored,
    Decoded,
    InvalidFrame,
};

enum class TeslaSignalValidity : std::uint8_t {
    Valid,
    Unavailable,
    Fault,
    Invalid,
};

struct BlindSpotDecodeValue {
    TeslaSignalValidity validity{TeslaSignalValidity::Unavailable};
    bool occupied{false};
    std::uint8_t warning_level{};
};

struct AutopilotDecodeValue {
    TeslaSignalValidity validity{TeslaSignalValidity::Unavailable};
    domain::AutopilotState state{domain::AutopilotState::Disabled};
};

struct SpeedLimitDecodeValue {
    TeslaSignalValidity validity{TeslaSignalValidity::Unavailable};
    std::optional<domain::SpeedLimit> value{};
    std::uint8_t raw_value{};
};

struct DriverAssistanceDecodeResult {
    DriverAssistanceDecodeStatus status{
        DriverAssistanceDecodeStatus::Ignored};
    BlindSpotDecodeValue left_blind_spot{};
    BlindSpotDecodeValue right_blind_spot{};
    AutopilotDecodeValue autopilot{};
    std::uint32_t source_timestamp_microseconds{};
    SpeedLimitDecodeValue speed_limit{};

    [[nodiscard]] constexpr bool decoded() const noexcept {
        return status == DriverAssistanceDecodeStatus::Decoded;
    }
};

// Decodes DAS_status (921) from DashKit's chassis bus. Tesla packs the
// Autopilot state and both rear blind-spot warning levels into byte zero. The
// fused speed limit occupies bits 8-12 and is scaled in 5-unit increments.
[[nodiscard]] DriverAssistanceDecodeResult decode_driver_assistance(
    const dashkit::protocol::CanFrameView& frame) noexcept;

}  // namespace tesla_headup::tesla
