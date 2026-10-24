#pragma once

#include "tesla_headup/domain/autopilot_state.hpp"
#include "tesla_headup/domain/battery_charge.hpp"
#include "tesla_headup/domain/blind_spots.hpp"
#include "tesla_headup/domain/selected_gear.hpp"
#include "tesla_headup/domain/power_flow.hpp"
#include "tesla_headup/domain/speed_limit.hpp"
#include "tesla_headup/domain/turn_signals.hpp"
#include "tesla_headup/domain/vehicle_speed.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

namespace tesla_headup::application {

using TelemetryTime = std::chrono::milliseconds;

struct TelemetryFreshnessPolicy {
    std::chrono::milliseconds vehicle_speed_timeout{500};
    std::chrono::milliseconds turn_signal_timeout{500};
    std::chrono::milliseconds turn_signal_hold{1'000};
    std::chrono::milliseconds blind_spot_timeout{500};
    // DAS_status has a 500 ms cycle; tolerate BLE jitter and missed updates.
    std::chrono::milliseconds autopilot_timeout{1'500};
    std::chrono::milliseconds selected_gear_timeout{500};
    // DAS_status itself is sent every 500 ms; allow delivery jitter/missed cycles.
    std::chrono::milliseconds speed_limit_timeout{1'500};
    std::chrono::milliseconds power_timeout{500};
    // Limits are a separate 100 ms CAN message. Allow bounded BLE delivery
    // gaps without extending the lifetime of the measured voltage/current.
    std::chrono::milliseconds power_limits_timeout{1'500};
    // Multiplexed 1 s energy message: retain each input across alternating muxes.
    std::chrono::milliseconds battery_charge_timeout{5'000};
};

enum class SignalStatus : std::uint8_t {
    NeverReceived,
    Fresh,
    Stale,
    Unavailable,
};

struct VehicleSpeedTelemetry {
    SignalStatus status{SignalStatus::NeverReceived};
    std::optional<domain::VehicleSpeed> current_value{};
    std::uint32_t source_timestamp_microseconds{};
    TelemetryTime received_at{};

    [[nodiscard]] constexpr bool fresh() const noexcept {
        return status == SignalStatus::Fresh && current_value.has_value();
    }
};

enum class TurnSignalLampSide : std::uint8_t {
    Left,
    Right,
};

struct TurnSignalTelemetry {
    domain::TurnSignals signals{domain::TurnSignals::off()};
    bool illuminated{false};
    SignalStatus left_status{SignalStatus::NeverReceived};
    SignalStatus right_status{SignalStatus::NeverReceived};
    std::uint32_t left_source_timestamp_microseconds{};
    std::uint32_t right_source_timestamp_microseconds{};
};

enum class BlindSpotSide : std::uint8_t {
    Left,
    Right,
};

struct BlindSpotTelemetry {
    domain::BlindSpots current_value{domain::BlindSpots::clear()};
    SignalStatus left_status{SignalStatus::NeverReceived};
    SignalStatus right_status{SignalStatus::NeverReceived};
    std::uint32_t left_source_timestamp_microseconds{};
    std::uint32_t right_source_timestamp_microseconds{};
};

struct AutopilotTelemetry {
    SignalStatus status{SignalStatus::NeverReceived};
    std::optional<domain::AutopilotState> current_value{};
    std::uint32_t source_timestamp_microseconds{};
    TelemetryTime received_at{};

    [[nodiscard]] constexpr bool fresh() const noexcept {
        return status == SignalStatus::Fresh && current_value.has_value();
    }

    [[nodiscard]] constexpr bool active() const noexcept {
        return fresh() && domain::autopilot_active(*current_value);
    }
};

struct SelectedGearTelemetry {
    SignalStatus status{SignalStatus::NeverReceived};
    std::optional<domain::SelectedGear> current_value{};
    std::uint32_t source_timestamp_microseconds{};
    TelemetryTime received_at{};

    [[nodiscard]] constexpr bool fresh() const noexcept {
        return status == SignalStatus::Fresh && current_value.has_value();
    }
};

struct SpeedLimitTelemetry {
    SignalStatus status{SignalStatus::NeverReceived};
    std::optional<domain::SpeedLimit> current_value{};
    std::uint32_t source_timestamp_microseconds{};
    TelemetryTime received_at{};

    [[nodiscard]] constexpr bool fresh() const noexcept {
        return status == SignalStatus::Fresh && current_value.has_value();
    }
};

// Holds the latest presentation-independent vehicle telemetry. Freshness uses
// the local monotonic receipt time; the wrapping DashKit timestamp is retained
// for traceability but is not suitable as the local expiry clock.
class TelemetryStore final {
public:
    [[nodiscard]] bool update_battery_energy(std::optional<domain::BatteryEnergy> value,
        std::uint32_t source_timestamp_microseconds, TelemetryTime received_at) noexcept;
    [[nodiscard]] bool update_battery_buffer(std::optional<std::uint32_t> centi_kwh,
        std::uint32_t source_timestamp_microseconds, TelemetryTime received_at) noexcept;
    [[nodiscard]] std::optional<domain::BatteryCharge> battery_charge(TelemetryTime now) const noexcept;
    [[nodiscard]] bool update_battery_electrical(
        std::optional<domain::BatteryElectricalState> value,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;
    [[nodiscard]] bool update_battery_power_limits(
        domain::BatteryPowerLimits value,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;
    [[nodiscard]] std::optional<domain::PowerFlow> power_flow(
        TelemetryTime now) const noexcept;

    explicit constexpr TelemetryStore(
        TelemetryFreshnessPolicy policy = {}) noexcept
        : policy_(policy) {}

    [[nodiscard]] bool update_vehicle_speed(
        domain::VehicleSpeed speed,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] bool mark_vehicle_speed_unavailable(
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] VehicleSpeedTelemetry vehicle_speed(
        TelemetryTime now) const noexcept;

    [[nodiscard]] bool update_turn_signal_lamp(
        TurnSignalLampSide side,
        bool illuminated,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] bool mark_turn_signal_lamp_unavailable(
        TurnSignalLampSide side,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] TurnSignalTelemetry turn_signals(
        TelemetryTime now) const noexcept;

    [[nodiscard]] bool update_blind_spot(
        BlindSpotSide side,
        bool occupied,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] bool mark_blind_spot_unavailable(
        BlindSpotSide side,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] BlindSpotTelemetry blind_spots(
        TelemetryTime now) const noexcept;

    [[nodiscard]] bool update_autopilot_state(
        domain::AutopilotState state,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] bool mark_autopilot_unavailable(
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] AutopilotTelemetry autopilot(
        TelemetryTime now) const noexcept;

    [[nodiscard]] bool update_selected_gear(
        domain::SelectedGear gear,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] bool mark_selected_gear_unavailable(
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] SelectedGearTelemetry selected_gear(
        TelemetryTime now) const noexcept;

    [[nodiscard]] bool update_speed_limit(
        domain::SpeedLimit speed_limit,
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] bool mark_speed_limit_unavailable(
        std::uint32_t source_timestamp_microseconds,
        TelemetryTime received_at) noexcept;

    [[nodiscard]] SpeedLimitTelemetry speed_limit(
        TelemetryTime now) const noexcept;

private:
    template <typename T>
    struct PowerSlot {
        std::optional<T> value{};
        std::uint32_t source_timestamp_microseconds{};
        TelemetryTime received_at{};
        SignalStatus status{SignalStatus::NeverReceived};
    };
    PowerSlot<domain::BatteryElectricalState> battery_electrical_{};
    PowerSlot<domain::BatteryEnergy> battery_energy_{};
    PowerSlot<std::uint32_t> battery_buffer_{};
    PowerSlot<domain::BatteryPowerLimits> battery_power_limits_{};

    struct TurnSignalLampSlot {
        bool illuminated{false};
        std::uint32_t source_timestamp_microseconds{};
        TelemetryTime received_at{};
        std::optional<TelemetryTime> last_illuminated_at{};
        SignalStatus status{SignalStatus::NeverReceived};
    };

    struct BooleanSignalSlot {
        bool value{false};
        std::uint32_t source_timestamp_microseconds{};
        TelemetryTime received_at{};
        SignalStatus status{SignalStatus::NeverReceived};
    };

    [[nodiscard]] bool accepts_vehicle_speed(
        TelemetryTime received_at) const noexcept;
    [[nodiscard]] TurnSignalLampSlot& turn_signal_slot(
        TurnSignalLampSide side) noexcept;
    [[nodiscard]] const TurnSignalLampSlot& turn_signal_slot(
        TurnSignalLampSide side) const noexcept;
    [[nodiscard]] SignalStatus turn_signal_status(
        const TurnSignalLampSlot& slot,
        TelemetryTime now) const noexcept;
    [[nodiscard]] bool turn_signal_active(
        const TurnSignalLampSlot& slot,
        SignalStatus status,
        TelemetryTime now) const noexcept;
    [[nodiscard]] BooleanSignalSlot& blind_spot_slot(
        BlindSpotSide side) noexcept;
    [[nodiscard]] const BooleanSignalSlot& blind_spot_slot(
        BlindSpotSide side) const noexcept;
    [[nodiscard]] static SignalStatus status_at(
        SignalStatus status,
        TelemetryTime received_at,
        TelemetryTime timeout,
        TelemetryTime now) noexcept;

    TelemetryFreshnessPolicy policy_;
    std::optional<domain::VehicleSpeed> vehicle_speed_{};
    std::uint32_t vehicle_speed_source_timestamp_{};
    TelemetryTime vehicle_speed_received_at_{};
    SignalStatus vehicle_speed_status_{SignalStatus::NeverReceived};
    TurnSignalLampSlot left_turn_signal_{};
    TurnSignalLampSlot right_turn_signal_{};
    BooleanSignalSlot left_blind_spot_{};
    BooleanSignalSlot right_blind_spot_{};
    std::optional<domain::AutopilotState> autopilot_state_{};
    std::uint32_t autopilot_source_timestamp_{};
    TelemetryTime autopilot_received_at_{};
    SignalStatus autopilot_status_{SignalStatus::NeverReceived};
    std::optional<domain::SelectedGear> selected_gear_{};
    std::uint32_t selected_gear_source_timestamp_{};
    TelemetryTime selected_gear_received_at_{};
    SignalStatus selected_gear_status_{SignalStatus::NeverReceived};
    std::optional<domain::SpeedLimit> speed_limit_{};
    std::uint32_t speed_limit_source_timestamp_{};
    TelemetryTime speed_limit_received_at_{};
    SignalStatus speed_limit_status_{SignalStatus::NeverReceived};
};

}  // namespace tesla_headup::application
