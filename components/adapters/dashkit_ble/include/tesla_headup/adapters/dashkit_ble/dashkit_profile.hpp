#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace tesla_headup::dashkit::profile {

inline constexpr std::string_view device_name{"DashKit"};

inline constexpr std::string_view can_service_uuid{
    "CADA0000-CA00-B1E0-B0D6-C000AA0100A1"};

inline constexpr std::string_view can_characteristic_uuid{
    "CADA0001-CA00-B1E0-B0D6-C000AA0100A1"};

inline constexpr std::string_view control_characteristic_uuid{
    "CADA0004-CA00-B1E0-B0D6-C000AA0100A1"};

inline constexpr std::string_view firmware_version_characteristic_uuid{
    "CADA0005-CA00-B1E0-B0D6-C000AA0100A1"};

inline constexpr std::array<std::uint8_t, 2> keepalive_command{0x45, 0x00};

// The keepalive command first shipped in DashKit 0.1.2. Older firmware does
// not understand opcode 0x45 and must remain on its original no-ping path.
inline constexpr std::array<std::uint16_t, 3> keepalive_since{0, 1, 2};

}  // namespace tesla_headup::dashkit::profile
