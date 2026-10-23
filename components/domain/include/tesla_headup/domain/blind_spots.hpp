#pragma once

namespace tesla_headup::domain {

class BlindSpots final {
public:
    [[nodiscard]] static constexpr BlindSpots clear() noexcept {
        return BlindSpots{false, false};
    }

    [[nodiscard]] static constexpr BlindSpots from_occupancy(
        bool left_occupied,
        bool right_occupied) noexcept {
        return BlindSpots{left_occupied, right_occupied};
    }

    [[nodiscard]] constexpr bool left_occupied() const noexcept {
        return left_occupied_;
    }

    [[nodiscard]] constexpr bool right_occupied() const noexcept {
        return right_occupied_;
    }

    friend constexpr bool operator==(const BlindSpots&, const BlindSpots&) =
        default;

private:
    constexpr BlindSpots(bool left_occupied, bool right_occupied) noexcept
        : left_occupied_(left_occupied), right_occupied_(right_occupied) {}

    bool left_occupied_;
    bool right_occupied_;
};

}  // namespace tesla_headup::domain
