#pragma once

#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"
#include "tesla_headup/domain/selected_gear.hpp"

#include <cstdint>
#include <optional>

namespace tesla_headup::tesla {

enum class GearDecodeStatus : std::uint8_t {
    Ignored,
    Decoded,
    Unavailable,
    InvalidFrame,
    InvalidValue,
};

struct GearDecodeResult {
    GearDecodeStatus status{GearDecodeStatus::Ignored};
    std::optional<domain::SelectedGear> gear{};
    std::uint32_t source_timestamp_microseconds{};

    [[nodiscard]] constexpr bool decoded() const noexcept {
        return status == GearDecodeStatus::Decoded;
    }
};

// Decodes DI_gear from DI_systemStatus (0x118) on DashKit bus 0.
// The little-endian DBC signal occupies bits 21..23.
[[nodiscard]] GearDecodeResult decode_selected_gear(
    const dashkit::protocol::CanFrameView& frame) noexcept;

}  // namespace tesla_headup::tesla
