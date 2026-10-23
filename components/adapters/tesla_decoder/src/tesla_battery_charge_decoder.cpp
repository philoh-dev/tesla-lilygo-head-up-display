#include "tesla_headup/adapters/tesla_decoder/tesla_battery_charge_decoder.hpp"

namespace tesla_headup::tesla {
BatteryChargeDecodeResult decode_battery_charge(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    BatteryChargeDecodeResult result{};
    result.source_timestamp_microseconds = frame.timestamp_microseconds;
    if (frame.bus != 1 || frame.address != 850) return result;
    if (frame.data.size() != 8) {
        result.status = BatteryChargeDecodeStatus::InvalidFrame;
        return result;
    }
    const auto word = [&frame](std::size_t index) {
        return static_cast<std::uint32_t>(frame.data[index]) |
               (static_cast<std::uint32_t>(frame.data[index + 1]) << 8U);
    };
    switch (frame.data[0] & 3U) {
    case 0: {
        const auto full = word(2), remaining = word(4);
        // SNA values are explicitly documented in the pinned DBC's VAL_ entries.
        result.status = full == 2047 || remaining == 2047
            ? BatteryChargeDecodeStatus::UnavailableEnergy : BatteryChargeDecodeStatus::Energy;
        result.energy = {full * 2U, remaining * 2U};
        break;
    }
    case 1:
        result.buffer_centi_kwh = word(2);
        result.status = result.buffer_centi_kwh == 255
            ? BatteryChargeDecodeStatus::UnavailableBuffer : BatteryChargeDecodeStatus::Buffer;
        break;
    default: break; // Other multiplexor values must not refresh either input.
    }
    return result;
}
}  // namespace tesla_headup::tesla
