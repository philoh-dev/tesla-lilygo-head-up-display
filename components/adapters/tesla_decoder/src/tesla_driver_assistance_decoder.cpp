#include "tesla_headup/adapters/tesla_decoder/tesla_driver_assistance_decoder.hpp"

#include <cstddef>
#include <cstdint>

namespace tesla_headup::tesla {
namespace {

constexpr std::uint8_t chassis_bus = 0;
constexpr std::uint32_t das_status_address = 921;
constexpr std::size_t das_status_payload_size = 8;

BlindSpotDecodeValue decode_blind_spot(std::uint8_t raw) noexcept {
    if (raw == 3U) {
        return {TeslaSignalValidity::Unavailable, false, raw};
    }
    return {TeslaSignalValidity::Valid, raw != 0U, raw};
}

AutopilotDecodeValue decode_autopilot(std::uint8_t raw) noexcept {
    using domain::AutopilotState;
    switch (raw) {
    case 0:
        return {TeslaSignalValidity::Valid, AutopilotState::Disabled};
    case 1:
        return {TeslaSignalValidity::Valid, AutopilotState::Unavailable};
    case 2:
        return {TeslaSignalValidity::Valid, AutopilotState::Available};
    case 3:
        return {TeslaSignalValidity::Valid, AutopilotState::ActiveNominal};
    case 4:
        return {TeslaSignalValidity::Valid, AutopilotState::ActiveRestricted};
    case 5:
        return {TeslaSignalValidity::Valid, AutopilotState::ActiveNavigation};
    case 6:
        return {
            TeslaSignalValidity::Valid,
            AutopilotState::ActiveFullSelfDriving,
        };
    case 8:
        return {TeslaSignalValidity::Valid, AutopilotState::Aborting};
    case 9:
        return {TeslaSignalValidity::Valid, AutopilotState::Aborted};
    case 14:
        return {TeslaSignalValidity::Fault, AutopilotState::Fault};
    case 15:
        return {
            TeslaSignalValidity::Unavailable,
            AutopilotState::Unavailable,
        };
    default:
        return {TeslaSignalValidity::Invalid, AutopilotState::Unavailable};
    }
}

SpeedLimitDecodeValue decode_speed_limit(std::uint8_t raw) noexcept {
    if (raw == 0U || raw == 31U) {
        return {TeslaSignalValidity::Unavailable, std::nullopt, raw};
    }
    const auto value = domain::SpeedLimit::from_display_value(
        static_cast<std::uint16_t>(raw) * 5U);
    return {
        value.has_value() ? TeslaSignalValidity::Valid
                          : TeslaSignalValidity::Invalid,
        value,
        raw,
    };
}

}  // namespace

DriverAssistanceDecodeResult decode_driver_assistance(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    if (frame.bus != chassis_bus || frame.address != das_status_address) {
        return {
            .status = DriverAssistanceDecodeStatus::Ignored,
            .source_timestamp_microseconds = frame.timestamp_microseconds,
        };
    }
    if (frame.data.size() != das_status_payload_size) {
        return {
            .status = DriverAssistanceDecodeStatus::InvalidFrame,
            .source_timestamp_microseconds = frame.timestamp_microseconds,
        };
    }

    const auto packed = frame.data.front();
    return {
        .status = DriverAssistanceDecodeStatus::Decoded,
        .left_blind_spot = decode_blind_spot(
            static_cast<std::uint8_t>((packed >> 4U) & 0x03U)),
        .right_blind_spot = decode_blind_spot(
            static_cast<std::uint8_t>((packed >> 6U) & 0x03U)),
        .autopilot = decode_autopilot(
            static_cast<std::uint8_t>(packed & 0x0FU)),
        .source_timestamp_microseconds = frame.timestamp_microseconds,
        .speed_limit = decode_speed_limit(
            static_cast<std::uint8_t>(frame.data[1] & 0x1FU)),
    };
}

}  // namespace tesla_headup::tesla
