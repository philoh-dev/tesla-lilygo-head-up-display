#pragma once

#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"
#include "tesla_headup/domain/power_flow.hpp"

namespace tesla_headup::tesla {

enum class PowerDecodeStatus : std::uint8_t {
    Ignored, Electrical, Limits, Unavailable, InvalidFrame,
};

struct PowerDecodeResult {
    PowerDecodeStatus status{PowerDecodeStatus::Ignored};
    domain::BatteryElectricalState electrical{};
    domain::BatteryPowerLimits limits{};
    std::uint32_t source_timestamp_microseconds{};
};

[[nodiscard]] PowerDecodeResult decode_battery_power(
    const dashkit::protocol::CanFrameView& frame) noexcept;

}  // namespace tesla_headup::tesla
