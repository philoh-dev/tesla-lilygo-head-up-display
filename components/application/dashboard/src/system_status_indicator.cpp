#include "tesla_headup/application/dashboard/system_status_indicator.hpp"

#include <string_view>

namespace tesla_headup::application {
namespace {

constexpr std::int16_t vertical_offset = -104;

std::string_view label(SystemStatus status) noexcept {
    switch (status) {
    case SystemStatus::Hidden:
        return {};
    case SystemStatus::Disconnected:
        return "DISCONNECTED";
    case SystemStatus::Connecting:
        return "CONNECTING";
    case SystemStatus::Reconnecting:
        return "RECONNECTING";
    case SystemStatus::PairingRequired:
        return "PAIRING REQUIRED";
    case SystemStatus::ConnectionFailed:
        return "CONNECTION FAILED";
    case SystemStatus::WaitingForData:
        return "WAITING FOR DATA";
    case SystemStatus::NoData:
        return "NO DATA";
    }
    return {};
}

TextRole role(SystemStatus status) noexcept {
    switch (status) {
    case SystemStatus::Connecting:
    case SystemStatus::WaitingForData:
        return TextRole::StatusInfo;
    case SystemStatus::Reconnecting:
    case SystemStatus::NoData:
        return TextRole::StatusWarning;
    case SystemStatus::Disconnected:
    case SystemStatus::PairingRequired:
    case SystemStatus::ConnectionFailed:
        return TextRole::StatusCritical;
    case SystemStatus::Hidden:
        return TextRole::StatusInfo;
    }
    return TextRole::StatusInfo;
}

}  // namespace

FeatureId SystemStatusIndicator::id() const noexcept {
    return FeatureId::SystemStatus;
}

void SystemStatusIndicator::render(DashboardCanvas& canvas) const {
    const auto text = label(status_);
    if (text.empty()) {
        return;
    }
    canvas.draw_text(TextElement{
        .feature = FeatureId::SystemStatus,
        .text = text,
        .role = role(status_),
        .vertical_offset = vertical_offset,
    });
}

void SystemStatusIndicator::set_status(SystemStatus status) noexcept {
    status_ = status;
}

SystemStatus SystemStatusIndicator::status() const noexcept {
    return status_;
}

}  // namespace tesla_headup::application
