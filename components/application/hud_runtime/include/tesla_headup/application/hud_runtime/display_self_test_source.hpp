#pragma once

#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

namespace tesla_headup::application {

enum class DisplaySelfTestPhase : std::uint8_t {
    Park,
    Accelerating,
    Cruising,
    LeftSignal,
    LeftBlindSpot,
    Autopilot,
    Decelerating,
    ReverseWait,
    ReverseAccelerating,
    FeatureSweep,
    AllFeatures,
};

struct DisplaySelfTestPolicy {
    std::chrono::milliseconds update_interval{100};
    std::chrono::milliseconds blink_interval{500};
};

// Produces deterministic, allocation-free telemetry for physical display
// bring-up. It deliberately writes through TelemetryStore so the real
// presenters, freshness rules, render scheduler, and canvas remain in use.
class DisplaySelfTestSource final {
public:
    explicit constexpr DisplaySelfTestSource(
        TelemetryStore& telemetry,
        DisplaySelfTestPolicy policy = {}) noexcept
        : telemetry_(telemetry), policy_(policy) {}

    [[nodiscard]] bool tick(TelemetryTime now) noexcept;
    [[nodiscard]] DisplaySelfTestPhase phase(TelemetryTime now) const noexcept;

private:
    [[nodiscard]] TelemetryTime elapsed_at(TelemetryTime now) const noexcept;
    void publish(
        DisplaySelfTestPhase phase,
        TelemetryTime elapsed,
        bool blink_illuminated,
        TelemetryTime now) noexcept;

    TelemetryStore& telemetry_;
    DisplaySelfTestPolicy policy_;
    std::optional<TelemetryTime> started_at_{};
    std::optional<TelemetryTime> last_update_at_{};
};

}  // namespace tesla_headup::application
