#pragma once

#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"
#include "tesla_headup/domain/battery_charge.hpp"

namespace tesla_headup::tesla {
enum class BatteryChargeDecodeStatus : std::uint8_t {
    Ignored, Energy, Buffer, UnavailableEnergy, UnavailableBuffer, InvalidFrame,
};
struct BatteryChargeDecodeResult {
    BatteryChargeDecodeStatus status{BatteryChargeDecodeStatus::Ignored};
    domain::BatteryEnergy energy{};
    std::uint32_t buffer_centi_kwh{};
    std::uint32_t source_timestamp_microseconds{};
};
[[nodiscard]] BatteryChargeDecodeResult decode_battery_charge(
    const dashkit::protocol::CanFrameView& frame) noexcept;
}  // namespace tesla_headup::tesla
