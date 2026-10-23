#pragma once

#include <cstdint>

namespace tesla_headup::domain {

enum class HighBeamMode : std::uint8_t {
    Off,
    Manual,
    Adaptive,
};

class ExteriorLights final {
public:
    [[nodiscard]] static constexpr ExteriorLights off() noexcept {
        return ExteriorLights{};
    }

    [[nodiscard]] static constexpr ExteriorLights from_lamps(
        bool position_lights,
        bool low_beam,
        HighBeamMode high_beam_mode,
        bool front_fog,
        bool rear_fog) noexcept {
        return ExteriorLights{
            position_lights,
            low_beam,
            high_beam_mode,
            front_fog,
            rear_fog,
        };
    }

    [[nodiscard]] constexpr bool position_lights_active() const noexcept {
        return position_lights_;
    }

    [[nodiscard]] constexpr bool low_beam_active() const noexcept {
        return low_beam_;
    }

    [[nodiscard]] constexpr bool high_beam_active() const noexcept {
        return high_beam_mode_ == HighBeamMode::Manual;
    }

    [[nodiscard]] constexpr bool adaptive_high_beam_active() const noexcept {
        return high_beam_mode_ == HighBeamMode::Adaptive;
    }

    [[nodiscard]] constexpr HighBeamMode high_beam_mode() const noexcept {
        return high_beam_mode_;
    }

    [[nodiscard]] constexpr bool front_fog_active() const noexcept {
        return front_fog_;
    }

    [[nodiscard]] constexpr bool rear_fog_active() const noexcept {
        return rear_fog_;
    }

    friend constexpr bool operator==(const ExteriorLights&, const ExteriorLights&) =
        default;

private:
    constexpr ExteriorLights() noexcept = default;

    constexpr ExteriorLights(
        bool position_lights,
        bool low_beam,
        HighBeamMode high_beam_mode,
        bool front_fog,
        bool rear_fog) noexcept
        : position_lights_(position_lights),
          low_beam_(low_beam),
          high_beam_mode_(high_beam_mode),
          front_fog_(front_fog),
          rear_fog_(rear_fog) {}

    bool position_lights_{false};
    bool low_beam_{false};
    HighBeamMode high_beam_mode_{HighBeamMode::Off};
    bool front_fog_{false};
    bool rear_fog_{false};
};

}  // namespace tesla_headup::domain
