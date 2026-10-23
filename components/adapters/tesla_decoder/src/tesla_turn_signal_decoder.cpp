#include "tesla_headup/adapters/tesla_decoder/tesla_turn_signal_decoder.hpp"

#include <cstddef>
#include <cstdint>

namespace tesla_headup::tesla {
namespace {

constexpr std::uint8_t vehicle_bus = 1;
constexpr std::uint32_t left_light_status_address = 994;
constexpr std::uint32_t right_light_status_address = 995;
constexpr std::size_t left_payload_size = 7;
constexpr std::size_t right_payload_size = 2;
constexpr std::uint8_t turn_signal_shift = 4;
constexpr std::uint8_t turn_signal_mask = 0x03;

enum class TeslaLampStatus : std::uint8_t {
    Off = 0,
    On = 1,
    Fault = 2,
    Unavailable = 3,
};

}  // namespace

TurnSignalDecodeResult decode_turn_signal_lamp(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    TurnSignalSide side;
    std::size_t expected_payload_size;
    if (frame.bus != vehicle_bus) {
        return {
            TurnSignalDecodeStatus::Ignored,
            TurnSignalSide::Left,
            false,
            frame.timestamp_microseconds,
        };
    }
    if (frame.address == left_light_status_address) {
        side = TurnSignalSide::Left;
        expected_payload_size = left_payload_size;
    } else if (frame.address == right_light_status_address) {
        side = TurnSignalSide::Right;
        expected_payload_size = right_payload_size;
    } else {
        return {
            TurnSignalDecodeStatus::Ignored,
            TurnSignalSide::Left,
            false,
            frame.timestamp_microseconds,
        };
    }

    // Some DashKit/CAN-controller combinations expose short classic-CAN
    // messages in an eight-byte storage frame. The signal is wholly contained
    // in the DBC-defined prefix, so accept trailing padding but never a
    // truncated payload.
    if (frame.data.size() < expected_payload_size) {
        return {
            TurnSignalDecodeStatus::InvalidFrame,
            side,
            false,
            frame.timestamp_microseconds,
        };
    }

    const auto raw_status = static_cast<TeslaLampStatus>(
        (frame.data.front() >> turn_signal_shift) & turn_signal_mask);
    switch (raw_status) {
    case TeslaLampStatus::Off:
        return {
            TurnSignalDecodeStatus::Decoded,
            side,
            false,
            frame.timestamp_microseconds,
        };
    case TeslaLampStatus::On:
        return {
            TurnSignalDecodeStatus::Decoded,
            side,
            true,
            frame.timestamp_microseconds,
        };
    case TeslaLampStatus::Fault:
        return {
            TurnSignalDecodeStatus::Fault,
            side,
            false,
            frame.timestamp_microseconds,
        };
    case TeslaLampStatus::Unavailable:
        return {
            TurnSignalDecodeStatus::Unavailable,
            side,
            false,
            frame.timestamp_microseconds,
        };
    }

    return {
        TurnSignalDecodeStatus::Unavailable,
        side,
        false,
        frame.timestamp_microseconds,
    };
}

}  // namespace tesla_headup::tesla
