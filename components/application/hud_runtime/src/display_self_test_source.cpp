#include "tesla_headup/application/hud_runtime/display_self_test_source.hpp"

#include "tesla_headup/domain/autopilot_state.hpp"
#include "tesla_headup/domain/selected_gear.hpp"
#include "tesla_headup/domain/speed_limit.hpp"
#include "tesla_headup/domain/vehicle_speed.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

namespace tesla_headup::application {
namespace {

using namespace std::chrono_literals;

constexpr auto park_end = 2s;
constexpr auto acceleration_end = 9s;
constexpr auto cruise_end = 14s;
constexpr auto left_signal_end = 15s;
constexpr auto left_blind_spot_end = 18s;
constexpr auto left_signal_after_blind_spot_end = 21s;
constexpr auto autopilot_end = 26s;
constexpr auto deceleration_end = 41s;
constexpr auto reverse_wait_end = 42s;
constexpr auto reverse_acceleration_end = 45s;
constexpr auto feature_active_duration = 1s;
constexpr auto feature_step_duration = 2'500ms;

enum class FeatureSweepStep : std::uint8_t {
    Speed,
    SelectedGear,
    LeftTurnSignal,
    RightTurnSignal,
    Hazard,
    LeftBlindSpot,
    RightBlindSpot,
    Autopilot,
    SpeedLimit,
    PowerUse,
    Regeneration,
};

constexpr std::array feature_sweep{
    FeatureSweepStep::Speed,
    FeatureSweepStep::SelectedGear,
    FeatureSweepStep::LeftTurnSignal,
    FeatureSweepStep::RightTurnSignal,
    FeatureSweepStep::Hazard,
    FeatureSweepStep::LeftBlindSpot,
    FeatureSweepStep::RightBlindSpot,
    FeatureSweepStep::Autopilot,
    FeatureSweepStep::SpeedLimit,
    FeatureSweepStep::PowerUse,
    FeatureSweepStep::Regeneration,
};

constexpr auto feature_sweep_duration =
    feature_step_duration * feature_sweep.size();
constexpr auto all_features_start =
    reverse_acceleration_end + feature_sweep_duration;

struct Scenario {
    std::optional<std::int16_t> battery_current_deciamps{};
    std::optional<std::uint16_t> speed_kmh{};
    std::optional<domain::SelectedGear> gear{};
    std::optional<std::uint16_t> speed_limit{};
    domain::AutopilotState autopilot{domain::AutopilotState::Disabled};
    bool left_signal{false};
    bool right_signal{false};
    bool left_blind_spot{false};
    bool right_blind_spot{false};
};

std::chrono::milliseconds bounded_positive(
    std::chrono::milliseconds value,
    std::chrono::milliseconds fallback) noexcept {
    return value > 0ms ? value : fallback;
}

std::uint64_t nonnegative_milliseconds(TelemetryTime value) noexcept {
    return value > TelemetryTime::zero()
               ? static_cast<std::uint64_t>(value.count())
               : 0U;
}

std::uint32_t source_timestamp(TelemetryTime now) noexcept {
    // The self-test has no originating CAN clock. Milliseconds preserve strict
    // ordering for almost 50 days before the uint32_t transport field wraps.
    return static_cast<std::uint32_t>(nonnegative_milliseconds(now));
}

std::uint16_t ramp_up(
    TelemetryTime elapsed,
    TelemetryTime start,
    TelemetryTime end,
    std::uint16_t target) noexcept {
    const auto position = nonnegative_milliseconds(elapsed - start);
    const auto duration = nonnegative_milliseconds(end - start);
    return static_cast<std::uint16_t>(
        (static_cast<std::uint64_t>(target) * position + duration / 2U) /
        duration);
}

std::uint16_t ramp_down(
    TelemetryTime elapsed,
    TelemetryTime start,
    TelemetryTime end,
    std::uint16_t initial) noexcept {
    return static_cast<std::uint16_t>(
        initial - ramp_up(elapsed, start, end, initial));
}

Scenario drive_scenario(
    DisplaySelfTestPhase phase,
    TelemetryTime elapsed) noexcept {
    Scenario scenario{};
    switch (phase) {
    case DisplaySelfTestPhase::Park:
        scenario.gear = domain::SelectedGear::Park;
        break;
    case DisplaySelfTestPhase::Accelerating:
        scenario.battery_current_deciamps = static_cast<std::int16_t>(
            ramp_up(elapsed, park_end, acceleration_end, 4000));
        scenario.speed_kmh = ramp_up(elapsed, park_end, acceleration_end, 130);
        scenario.gear = domain::SelectedGear::Drive;
        scenario.speed_limit = 50;
        break;
    case DisplaySelfTestPhase::Cruising:
        scenario.speed_kmh = 130;
        scenario.gear = domain::SelectedGear::Drive;
        scenario.speed_limit = 50;
        break;
    case DisplaySelfTestPhase::LeftSignal:
        scenario.speed_kmh = 130;
        scenario.gear = domain::SelectedGear::Drive;
        scenario.speed_limit = 50;
        scenario.left_signal = true;
        break;
    case DisplaySelfTestPhase::LeftBlindSpot:
        scenario.speed_kmh = 130;
        scenario.gear = domain::SelectedGear::Drive;
        scenario.speed_limit = 50;
        scenario.left_signal = true;
        scenario.left_blind_spot = true;
        break;
    case DisplaySelfTestPhase::Autopilot:
        scenario.speed_kmh = 130;
        scenario.gear = domain::SelectedGear::Drive;
        scenario.speed_limit = 50;
        scenario.autopilot = domain::AutopilotState::ActiveFullSelfDriving;
        break;
    case DisplaySelfTestPhase::Decelerating:
        scenario.battery_current_deciamps = -1000;
        scenario.speed_kmh = ramp_down(
            elapsed,
            autopilot_end,
            deceleration_end,
            130);
        scenario.gear = domain::SelectedGear::Drive;
        scenario.speed_limit = 50;
        break;
    case DisplaySelfTestPhase::ReverseWait:
        scenario.speed_kmh = 0;
        scenario.gear = domain::SelectedGear::Drive;
        scenario.speed_limit = 50;
        break;
    case DisplaySelfTestPhase::ReverseAccelerating:
        scenario.battery_current_deciamps = 500;
        scenario.speed_kmh = ramp_up(
            elapsed, reverse_wait_end, reverse_acceleration_end, 15);
        scenario.gear = domain::SelectedGear::Reverse;
        scenario.speed_limit = 50;
        break;
    case DisplaySelfTestPhase::FeatureSweep:
    case DisplaySelfTestPhase::AllFeatures:
        break;
    }
    return scenario;
}

void enable_sweep_feature(Scenario& scenario, FeatureSweepStep feature) {
    switch (feature) {
    case FeatureSweepStep::Speed:
        scenario.speed_kmh = 88;
        break;
    case FeatureSweepStep::SelectedGear:
        scenario.gear = domain::SelectedGear::Drive;
        break;
    case FeatureSweepStep::LeftTurnSignal:
        scenario.left_signal = true;
        break;
    case FeatureSweepStep::RightTurnSignal:
        scenario.right_signal = true;
        break;
    case FeatureSweepStep::Hazard:
        scenario.left_signal = true;
        scenario.right_signal = true;
        break;
    case FeatureSweepStep::LeftBlindSpot:
        scenario.left_blind_spot = true;
        break;
    case FeatureSweepStep::RightBlindSpot:
        scenario.right_blind_spot = true;
        break;
    case FeatureSweepStep::Autopilot:
        scenario.autopilot = domain::AutopilotState::ActiveFullSelfDriving;
        break;
    case FeatureSweepStep::SpeedLimit:
        scenario.speed_limit = 50;
        break;
    case FeatureSweepStep::PowerUse:
        scenario.battery_current_deciamps = 4000;
        break;
    case FeatureSweepStep::Regeneration:
        scenario.battery_current_deciamps = -1000;
        break;
    }
}

Scenario scenario_at(
    DisplaySelfTestPhase phase,
    TelemetryTime elapsed) noexcept {
    if (phase != DisplaySelfTestPhase::FeatureSweep &&
        phase != DisplaySelfTestPhase::AllFeatures) {
        return drive_scenario(phase, elapsed);
    }

    Scenario scenario{};
    if (phase == DisplaySelfTestPhase::FeatureSweep) {
        const auto sweep_elapsed = elapsed - reverse_acceleration_end;
        const auto step = static_cast<std::size_t>(
            sweep_elapsed / feature_step_duration);
        const auto within_step = sweep_elapsed % feature_step_duration;
        if (step < feature_sweep.size() &&
            within_step < feature_active_duration) {
            enable_sweep_feature(scenario, feature_sweep[step]);
        }
        return scenario;
    }

    scenario.speed_kmh = 130;
    scenario.battery_current_deciamps = 2000;
    scenario.gear = domain::SelectedGear::Drive;
    scenario.speed_limit = 50;
    scenario.autopilot = domain::AutopilotState::ActiveFullSelfDriving;
    scenario.left_signal = true;
    scenario.right_signal = true;
    scenario.left_blind_spot = true;
    scenario.right_blind_spot = true;
    return scenario;
}

}  // namespace

bool DisplaySelfTestSource::tick(TelemetryTime now) noexcept {
    if (!started_at_.has_value()) {
        started_at_ = now;
    }

    const auto interval = bounded_positive(policy_.update_interval, 100ms);
    if (last_update_at_.has_value() && now >= *last_update_at_ &&
        (now - *last_update_at_) < interval) {
        return false;
    }

    if (last_update_at_.has_value() && now < *last_update_at_) {
        started_at_ = now;
    }

    const auto blink_interval = bounded_positive(policy_.blink_interval, 500ms);
    const auto elapsed = elapsed_at(now);
    const bool blink_illuminated =
        (nonnegative_milliseconds(elapsed) /
         static_cast<std::uint64_t>(blink_interval.count())) %
            2U ==
        0U;
    publish(phase(now), elapsed, blink_illuminated, now);
    last_update_at_ = now;
    return true;
}

TelemetryTime DisplaySelfTestSource::elapsed_at(TelemetryTime now) const
    noexcept {
    if (!started_at_.has_value() || now < *started_at_) {
        return TelemetryTime::zero();
    }
    return now - *started_at_;
}

DisplaySelfTestPhase DisplaySelfTestSource::phase(
    TelemetryTime now) const noexcept {
    const auto elapsed = elapsed_at(now);
    if (elapsed < park_end) {
        return DisplaySelfTestPhase::Park;
    }
    if (elapsed < acceleration_end) {
        return DisplaySelfTestPhase::Accelerating;
    }
    if (elapsed < cruise_end) {
        return DisplaySelfTestPhase::Cruising;
    }
    if (elapsed < left_signal_end) {
        return DisplaySelfTestPhase::LeftSignal;
    }
    if (elapsed < left_blind_spot_end) {
        return DisplaySelfTestPhase::LeftBlindSpot;
    }
    if (elapsed < left_signal_after_blind_spot_end) {
        return DisplaySelfTestPhase::LeftSignal;
    }
    if (elapsed < autopilot_end) {
        return DisplaySelfTestPhase::Autopilot;
    }
    if (elapsed < deceleration_end) {
        return DisplaySelfTestPhase::Decelerating;
    }
    if (elapsed < reverse_wait_end) {
        return DisplaySelfTestPhase::ReverseWait;
    }
    if (elapsed < reverse_acceleration_end) {
        return DisplaySelfTestPhase::ReverseAccelerating;
    }
    if (elapsed < all_features_start) {
        return DisplaySelfTestPhase::FeatureSweep;
    }
    return DisplaySelfTestPhase::AllFeatures;
}

void DisplaySelfTestSource::publish(
    DisplaySelfTestPhase current_phase,
    TelemetryTime elapsed,
    bool blink_illuminated,
    TelemetryTime now) noexcept {
    const Scenario scenario = scenario_at(current_phase, elapsed);
    const auto timestamp = source_timestamp(now);
    (void)telemetry_.update_battery_power_limits({32000, 8000}, timestamp, now);
    // Synthetic 51%: (41.76 - 3.0) / (79.0 - 3.0).
    (void)telemetry_.update_battery_energy(domain::BatteryEnergy{7900, 4176}, timestamp, now);
    (void)telemetry_.update_battery_buffer(300, timestamp, now);
    if (scenario.battery_current_deciamps) {
        (void)telemetry_.update_battery_electrical(
            domain::BatteryElectricalState{40000, *scenario.battery_current_deciamps},
            timestamp, now);
    } else {
        (void)telemetry_.update_battery_electrical(std::nullopt, timestamp, now);
    }
    if (scenario.speed_kmh.has_value()) {
        const auto speed = domain::VehicleSpeed::from_kilometres_per_hour(
            *scenario.speed_kmh);
        if (speed.has_value()) {
            (void)telemetry_.update_vehicle_speed(*speed, timestamp, now);
        }
    } else {
        (void)telemetry_.mark_vehicle_speed_unavailable(timestamp, now);
    }
    if (scenario.gear.has_value()) {
        (void)telemetry_.update_selected_gear(*scenario.gear, timestamp, now);
    } else {
        (void)telemetry_.mark_selected_gear_unavailable(timestamp, now);
    }
    if (scenario.speed_limit.has_value()) {
        const auto limit = domain::SpeedLimit::from_display_value(
            *scenario.speed_limit);
        if (limit.has_value()) {
            (void)telemetry_.update_speed_limit(*limit, timestamp, now);
        }
    } else {
        (void)telemetry_.mark_speed_limit_unavailable(timestamp, now);
    }
    (void)telemetry_.update_autopilot_state(
        scenario.autopilot, timestamp, now);
    (void)telemetry_.update_turn_signal_lamp(
        TurnSignalLampSide::Left,
        scenario.left_signal && blink_illuminated,
        timestamp,
        now);
    (void)telemetry_.update_turn_signal_lamp(
        TurnSignalLampSide::Right,
        scenario.right_signal && blink_illuminated,
        timestamp,
        now);
    (void)telemetry_.update_blind_spot(
        BlindSpotSide::Left, scenario.left_blind_spot, timestamp, now);
    (void)telemetry_.update_blind_spot(
        BlindSpotSide::Right, scenario.right_blind_spot, timestamp, now);
}

}  // namespace tesla_headup::application
