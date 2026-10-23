#include "tesla_headup/application/dashboard/blind_spot_warnings.hpp"

namespace tesla_headup::application {
namespace {

constexpr std::uint16_t warning_width = 76;

EdgeGradientElement warning(DisplayEdge edge) {
    return EdgeGradientElement{
        .feature = FeatureId::BlindSpotWarnings,
        .edge = edge,
        .role = GradientRole::BlindSpotWarning,
        .width = warning_width,
    };
}

}  // namespace

FeatureId BlindSpotWarnings::id() const noexcept {
    return FeatureId::BlindSpotWarnings;
}

void BlindSpotWarnings::render(DashboardCanvas& canvas) const {
    if (blind_spots_.left_occupied()) {
        canvas.draw_edge_gradient(warning(DisplayEdge::Left));
    }
    if (blind_spots_.right_occupied()) {
        canvas.draw_edge_gradient(warning(DisplayEdge::Right));
    }
}

void BlindSpotWarnings::set_blind_spots(
    domain::BlindSpots blind_spots) noexcept {
    blind_spots_ = blind_spots;
}

domain::BlindSpots BlindSpotWarnings::blind_spots() const noexcept {
    return blind_spots_;
}

}  // namespace tesla_headup::application
