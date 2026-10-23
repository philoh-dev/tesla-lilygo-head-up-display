#pragma once

#include <cstdint>

namespace tesla_headup::domain {

enum class TurnSignalState : std::uint8_t {
    Off,
    Left,
    Right,
    Hazard,
};

class TurnSignals final {
public:
    [[nodiscard]] static constexpr TurnSignals off() noexcept {
        return TurnSignals{TurnSignalState::Off};
    }

    [[nodiscard]] static constexpr TurnSignals left() noexcept {
        return TurnSignals{TurnSignalState::Left};
    }

    [[nodiscard]] static constexpr TurnSignals right() noexcept {
        return TurnSignals{TurnSignalState::Right};
    }

    [[nodiscard]] static constexpr TurnSignals hazard() noexcept {
        return TurnSignals{TurnSignalState::Hazard};
    }

    [[nodiscard]] static constexpr TurnSignals from_lamps(
        bool left_active,
        bool right_active) noexcept {
        if (left_active && right_active) {
            return hazard();
        }
        if (left_active) {
            return left();
        }
        if (right_active) {
            return right();
        }
        return off();
    }

    [[nodiscard]] constexpr TurnSignalState state() const noexcept {
        return state_;
    }

    [[nodiscard]] constexpr bool left_active() const noexcept {
        return state_ == TurnSignalState::Left ||
               state_ == TurnSignalState::Hazard;
    }

    [[nodiscard]] constexpr bool right_active() const noexcept {
        return state_ == TurnSignalState::Right ||
               state_ == TurnSignalState::Hazard;
    }

    friend constexpr bool operator==(const TurnSignals&, const TurnSignals&) =
        default;

private:
    explicit constexpr TurnSignals(TurnSignalState state) noexcept
        : state_(state) {}

    TurnSignalState state_;
};

}  // namespace tesla_headup::domain
