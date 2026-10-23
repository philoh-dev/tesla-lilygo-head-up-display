#include "tesla_headup/adapters/tesla_decoder/tesla_power_decoder.hpp"

namespace tesla_headup::tesla {

PowerDecodeResult decode_battery_power(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    PowerDecodeResult result{};
    result.source_timestamp_microseconds = frame.timestamp_microseconds;
    if (frame.bus != 1 || (frame.address != 306 && frame.address != 594)) {
        return result;
    }
    // Like DashPilot's signal reader, require the bytes used by these fields,
    // not unrelated trailing signals from the nominal eight-byte DBC message.
    // Voltage/current and both limits are entirely contained in bytes 0..3.
    if (frame.data.size() < 4 || frame.data.size() > 8) {
        result.status = PowerDecodeStatus::InvalidFrame;
        return result;
    }
    const auto word = [&frame](std::size_t index) {
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(frame.data[index]) |
            (static_cast<std::uint16_t>(frame.data[index + 1]) << 8U));
    };
    if (frame.address == 594) {
        result.status = PowerDecodeStatus::Limits;
        result.limits = {word(2), word(0)};
        return result;
    }
    // BMS_packCurrent is signed 15-bit little-endian, scaled by -0.1 A.
    const auto raw = static_cast<std::int32_t>(word(2) & 0x7FFFU);
    const auto signed_raw = (raw & 0x4000) != 0 ? raw - 0x8000 : raw;
    if (signed_raw == -16384 || word(0) == 0) {
        result.status = PowerDecodeStatus::Unavailable;
        return result;
    }
    result.status = PowerDecodeStatus::Electrical;
    result.electrical = {word(0), static_cast<std::int16_t>(-signed_raw)};
    return result;
}

}  // namespace tesla_headup::tesla
