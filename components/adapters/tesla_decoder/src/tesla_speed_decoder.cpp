#include "tesla_headup/adapters/tesla_decoder/tesla_speed_decoder.hpp"

#include <cstdint>

namespace tesla_headup::tesla {
namespace {

constexpr std::uint8_t vehicle_bus = 0;
constexpr std::uint32_t di_speed_address = 599;
constexpr std::size_t di_speed_payload_size = 8;
constexpr std::uint8_t ui_speed_unavailable = 0xFF;

}  // namespace

SpeedDecodeResult decode_vehicle_speed(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    if (frame.bus != vehicle_bus || frame.address != di_speed_address) {
        return {
            SpeedDecodeStatus::Ignored,
            std::nullopt,
            frame.timestamp_microseconds,
        };
    }

    if (frame.data.size() != di_speed_payload_size) {
        return {
            SpeedDecodeStatus::InvalidFrame,
            std::nullopt,
            frame.timestamp_microseconds,
        };
    }

    // Match DashPilot's dashboard source: DI_uiSpeed, bits 24..31.
    // This adapter's supported vehicle profile is km/h. Do not infer source
    // units from DI_uiSpeedUnits: on the target metric car it caused 16 -> 26.
    // Presentation units remain independent of the vehicle's source profile.
    const auto raw_speed = frame.data[3];
    if (raw_speed == ui_speed_unavailable) {
        return {
            SpeedDecodeStatus::Unavailable,
            std::nullopt,
            frame.timestamp_microseconds,
        };
    }

    const auto speed =
        domain::VehicleSpeed::from_kilometres_per_hour(raw_speed);
    if (!speed.has_value()) {
        return {
            SpeedDecodeStatus::OutOfRange,
            std::nullopt,
            frame.timestamp_microseconds,
        };
    }

    return {
        SpeedDecodeStatus::Decoded,
        speed,
        frame.timestamp_microseconds,
    };
}

}  // namespace tesla_headup::tesla
