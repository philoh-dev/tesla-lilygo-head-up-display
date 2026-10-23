#pragma once

#include <cstdint>

namespace tesla_headup::domain {

enum class AutopilotState : std::uint8_t {
    Disabled = 0,
    Unavailable = 1,
    Available = 2,
    ActiveNominal = 3,
    ActiveRestricted = 4,
    ActiveNavigation = 5,
    ActiveFullSelfDriving = 6,
    Aborting = 8,
    Aborted = 9,
    Fault = 14,
};

[[nodiscard]] constexpr bool autopilot_active(
    AutopilotState state) noexcept {
    return state == AutopilotState::ActiveNominal ||
           state == AutopilotState::ActiveRestricted ||
           state == AutopilotState::ActiveNavigation ||
           state == AutopilotState::ActiveFullSelfDriving;
}

}  // namespace tesla_headup::domain
