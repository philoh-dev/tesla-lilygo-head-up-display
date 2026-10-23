#pragma once

#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"

#include <cstdint>

namespace tesla_headup::tesla {

enum class TurnSignalSide : std::uint8_t {
    Left,
    Right,
};

enum class TurnSignalDecodeStatus : std::uint8_t {
    Ignored,
    Decoded,
    Fault,
    Unavailable,
    InvalidFrame,
};

struct TurnSignalDecodeResult {
    TurnSignalDecodeStatus status{TurnSignalDecodeStatus::Ignored};
    TurnSignalSide side{TurnSignalSide::Left};
    bool lamp_illuminated{false};
    std::uint32_t source_timestamp_microseconds{};

    [[nodiscard]] constexpr bool decoded() const noexcept {
        return status == TurnSignalDecodeStatus::Decoded;
    }
};

// Decodes the physical turn-lamp state from VCLEFT_lightStatus (994) and
// VCRIGHT_lightStatus (995) on DashKit bus 1. DBC-sized payloads and classic
// CAN frames with trailing adapter padding are accepted.
[[nodiscard]] TurnSignalDecodeResult decode_turn_signal_lamp(
    const dashkit::protocol::CanFrameView& frame) noexcept;

}  // namespace tesla_headup::tesla
