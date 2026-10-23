#pragma once

#include <cstdint>

namespace tesla_headup::domain {

enum class SelectedGear : std::uint8_t {
    Park,
    Reverse,
    Neutral,
    Drive,
};

[[nodiscard]] constexpr char selected_gear_label(
    SelectedGear gear) noexcept {
    switch (gear) {
    case SelectedGear::Park:
        return 'P';
    case SelectedGear::Reverse:
        return 'R';
    case SelectedGear::Neutral:
        return 'N';
    case SelectedGear::Drive:
        return 'D';
    }
    return '?';
}

}  // namespace tesla_headup::domain
