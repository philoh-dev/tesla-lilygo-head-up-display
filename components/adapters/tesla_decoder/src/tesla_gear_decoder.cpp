#include "tesla_headup/adapters/tesla_decoder/tesla_gear_decoder.hpp"

namespace tesla_headup::tesla {
namespace {

constexpr std::uint8_t vehicle_bus = 0;
constexpr std::uint32_t di_system_status_address = 0x118;
constexpr std::size_t di_system_status_payload_size = 8;
constexpr std::size_t gear_byte = 2;
constexpr std::uint8_t gear_shift = 5;
constexpr std::uint8_t gear_mask = 0x07;

}  // namespace

GearDecodeResult decode_selected_gear(
    const dashkit::protocol::CanFrameView& frame) noexcept {
    if (frame.bus != vehicle_bus ||
        frame.address != di_system_status_address) {
        return {
            GearDecodeStatus::Ignored,
            std::nullopt,
            frame.timestamp_microseconds,
        };
    }
    if (frame.data.size() != di_system_status_payload_size) {
        return {
            GearDecodeStatus::InvalidFrame,
            std::nullopt,
            frame.timestamp_microseconds,
        };
    }

    const auto raw = static_cast<std::uint8_t>(
        (frame.data[gear_byte] >> gear_shift) & gear_mask);
    const auto decoded = [raw]() -> std::optional<domain::SelectedGear> {
        switch (raw) {
        case 1:
            return domain::SelectedGear::Park;
        case 2:
            return domain::SelectedGear::Reverse;
        case 3:
            return domain::SelectedGear::Neutral;
        case 4:
            return domain::SelectedGear::Drive;
        default:
            return std::nullopt;
        }
    }();
    if (decoded.has_value()) {
        return {
            GearDecodeStatus::Decoded,
            decoded,
            frame.timestamp_microseconds,
        };
    }
    if (raw == 0 || raw == 7) {
        return {
            GearDecodeStatus::Unavailable,
            std::nullopt,
            frame.timestamp_microseconds,
        };
    }
    return {
        GearDecodeStatus::InvalidValue,
        std::nullopt,
        frame.timestamp_microseconds,
    };
}

}  // namespace tesla_headup::tesla
