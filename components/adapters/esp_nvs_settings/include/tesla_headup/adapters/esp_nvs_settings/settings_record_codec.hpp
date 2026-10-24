#pragma once

#include "tesla_headup/domain/settings.hpp"

#include <cstdint>
#include <optional>

namespace tesla_headup::platform {

struct SettingsRecord {
    static constexpr std::uint8_t current_version = 2;

    std::uint8_t version{current_version};
    std::uint8_t units{0};
    std::uint8_t projection_mirrored{1};
    std::uint8_t brightness_percent{100};
    std::uint8_t enabled_feature_bits{0x3B};

    friend constexpr bool operator==(
        const SettingsRecord&,
        const SettingsRecord&) = default;
};

[[nodiscard]] SettingsRecord encode_settings(
    const domain::Settings& settings) noexcept;

[[nodiscard]] std::optional<domain::Settings> decode_settings(
    SettingsRecord record) noexcept;

}  // namespace tesla_headup::platform
