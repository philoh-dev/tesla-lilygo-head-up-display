#pragma once

#include "tesla_headup/application/dashboard/dashboard_feature.hpp"

#include <cstdint>

namespace tesla_headup::application {

enum class SystemStatus : std::uint8_t {
    Hidden,
    Disconnected,
    Connecting,
    Reconnecting,
    PairingRequired,
    ConnectionFailed,
    WaitingForData,
    NoData,
};

class SystemStatusIndicator final : public DashboardFeature {
public:
    [[nodiscard]] FeatureId id() const noexcept override;
    void render(DashboardCanvas& canvas) const override;

    void set_status(SystemStatus status) noexcept;
    [[nodiscard]] SystemStatus status() const noexcept;

private:
    SystemStatus status_{SystemStatus::Disconnected};
};

}  // namespace tesla_headup::application
