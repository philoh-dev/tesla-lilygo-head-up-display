#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <chrono>

namespace tesla_headup::application {

bool TelemetryStore::update_battery_energy(std::optional<domain::BatteryEnergy> value,
    std::uint32_t timestamp, TelemetryTime received_at) noexcept {
    auto& slot = battery_energy_;
    if (slot.status != SignalStatus::NeverReceived && received_at < slot.received_at) return false;
    slot = {value, timestamp, received_at, value ? SignalStatus::Fresh : SignalStatus::Unavailable};
    return true;
}

bool TelemetryStore::update_battery_buffer(std::optional<std::uint32_t> value,
    std::uint32_t timestamp, TelemetryTime received_at) noexcept {
    auto& slot = battery_buffer_;
    if (slot.status != SignalStatus::NeverReceived && received_at < slot.received_at) return false;
    slot = {value, timestamp, received_at, value ? SignalStatus::Fresh : SignalStatus::Unavailable};
    return true;
}

std::optional<domain::BatteryCharge> TelemetryStore::battery_charge(TelemetryTime now) const noexcept {
    if (!battery_energy_.value || !battery_buffer_.value ||
        status_at(battery_energy_.status, battery_energy_.received_at, policy_.battery_charge_timeout, now) != SignalStatus::Fresh ||
        status_at(battery_buffer_.status, battery_buffer_.received_at, policy_.battery_charge_timeout, now) != SignalStatus::Fresh)
        return std::nullopt;
    return domain::BatteryCharge::from_energy(*battery_energy_.value, *battery_buffer_.value);
}

bool TelemetryStore::update_battery_electrical(
    std::optional<domain::BatteryElectricalState> value,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    auto& slot = battery_electrical_;
    if (slot.status != SignalStatus::NeverReceived &&
        received_at < slot.received_at) {
        return false;
    }
    slot = {
        value,
        source_timestamp_microseconds,
        received_at,
        value.has_value() ? SignalStatus::Fresh : SignalStatus::Unavailable};
    return true;
}

bool TelemetryStore::update_battery_power_limits(
    domain::BatteryPowerLimits value,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    auto& slot = battery_power_limits_;
    if (slot.status != SignalStatus::NeverReceived &&
        received_at < slot.received_at) {
        return false;
    }
    slot = {
        value, source_timestamp_microseconds, received_at, SignalStatus::Fresh};
    return true;
}

std::optional<domain::PowerFlow> TelemetryStore::power_flow(
    TelemetryTime now) const noexcept {
    if (status_at(battery_electrical_.status, battery_electrical_.received_at,
                  policy_.power_timeout, now) != SignalStatus::Fresh ||
        status_at(battery_power_limits_.status, battery_power_limits_.received_at,
                  policy_.power_limits_timeout, now) != SignalStatus::Fresh ||
        !battery_electrical_.value || !battery_power_limits_.value) {
        return std::nullopt;
    }
    return domain::PowerFlow::from_battery(
        *battery_electrical_.value, *battery_power_limits_.value);
}

bool TelemetryStore::update_speed_limit(
    domain::SpeedLimit speed_limit,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    if (speed_limit_status_ != SignalStatus::NeverReceived &&
        received_at < speed_limit_received_at_) {
        return false;
    }
    speed_limit_ = speed_limit;
    speed_limit_source_timestamp_ = source_timestamp_microseconds;
    speed_limit_received_at_ = received_at;
    speed_limit_status_ = SignalStatus::Fresh;
    return true;
}

bool TelemetryStore::mark_speed_limit_unavailable(
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    if (speed_limit_status_ != SignalStatus::NeverReceived &&
        received_at < speed_limit_received_at_) {
        return false;
    }
    speed_limit_.reset();
    speed_limit_source_timestamp_ = source_timestamp_microseconds;
    speed_limit_received_at_ = received_at;
    speed_limit_status_ = SignalStatus::Unavailable;
    return true;
}

SpeedLimitTelemetry TelemetryStore::speed_limit(
    TelemetryTime now) const noexcept {
    const auto status = status_at(
        speed_limit_status_,
        speed_limit_received_at_,
        policy_.speed_limit_timeout,
        now);
    return {
        status,
        status == SignalStatus::Fresh ? speed_limit_ : std::nullopt,
        speed_limit_source_timestamp_,
        speed_limit_received_at_,
    };
}

bool TelemetryStore::update_selected_gear(
    domain::SelectedGear gear,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    if (selected_gear_status_ != SignalStatus::NeverReceived &&
        received_at < selected_gear_received_at_) {
        return false;
    }
    selected_gear_ = gear;
    selected_gear_source_timestamp_ = source_timestamp_microseconds;
    selected_gear_received_at_ = received_at;
    selected_gear_status_ = SignalStatus::Fresh;
    return true;
}

bool TelemetryStore::mark_selected_gear_unavailable(
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    if (selected_gear_status_ != SignalStatus::NeverReceived &&
        received_at < selected_gear_received_at_) {
        return false;
    }
    selected_gear_.reset();
    selected_gear_source_timestamp_ = source_timestamp_microseconds;
    selected_gear_received_at_ = received_at;
    selected_gear_status_ = SignalStatus::Unavailable;
    return true;
}

SelectedGearTelemetry TelemetryStore::selected_gear(
    TelemetryTime now) const noexcept {
    const auto status = status_at(
        selected_gear_status_,
        selected_gear_received_at_,
        policy_.selected_gear_timeout,
        now);
    return {
        status,
        status == SignalStatus::Fresh ? selected_gear_ : std::nullopt,
        selected_gear_source_timestamp_,
        selected_gear_received_at_,
    };
}

bool TelemetryStore::update_vehicle_speed(
    domain::VehicleSpeed speed,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    if (!accepts_vehicle_speed(received_at)) {
        return false;
    }

    vehicle_speed_ = speed;
    vehicle_speed_source_timestamp_ = source_timestamp_microseconds;
    vehicle_speed_received_at_ = received_at;
    vehicle_speed_status_ = SignalStatus::Fresh;
    return true;
}

bool TelemetryStore::mark_vehicle_speed_unavailable(
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    if (!accepts_vehicle_speed(received_at)) {
        return false;
    }

    vehicle_speed_.reset();
    vehicle_speed_source_timestamp_ = source_timestamp_microseconds;
    vehicle_speed_received_at_ = received_at;
    vehicle_speed_status_ = SignalStatus::Unavailable;
    return true;
}

VehicleSpeedTelemetry TelemetryStore::vehicle_speed(
    TelemetryTime now) const noexcept {
    if (vehicle_speed_status_ == SignalStatus::NeverReceived ||
        vehicle_speed_status_ == SignalStatus::Unavailable) {
        return {
            vehicle_speed_status_,
            std::nullopt,
            vehicle_speed_source_timestamp_,
            vehicle_speed_received_at_,
        };
    }

    const auto timeout = policy_.vehicle_speed_timeout < TelemetryTime::zero()
                             ? TelemetryTime::zero()
                             : policy_.vehicle_speed_timeout;
    const bool clock_precedes_observation = now < vehicle_speed_received_at_;
    const bool within_timeout =
        clock_precedes_observation ||
        (now - vehicle_speed_received_at_) <= timeout;
    if (!within_timeout) {
        return {
            SignalStatus::Stale,
            std::nullopt,
            vehicle_speed_source_timestamp_,
            vehicle_speed_received_at_,
        };
    }

    return {
        SignalStatus::Fresh,
        vehicle_speed_,
        vehicle_speed_source_timestamp_,
        vehicle_speed_received_at_,
    };
}

bool TelemetryStore::update_turn_signal_lamp(
    TurnSignalLampSide side,
    bool illuminated,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    auto& slot = turn_signal_slot(side);
    if (slot.status != SignalStatus::NeverReceived &&
        received_at < slot.received_at) {
        return false;
    }

    slot.illuminated = illuminated;
    slot.source_timestamp_microseconds = source_timestamp_microseconds;
    slot.received_at = received_at;
    slot.status = SignalStatus::Fresh;
    if (illuminated) {
        slot.last_illuminated_at = received_at;
        auto& opposite = turn_signal_slot(
            side == TurnSignalLampSide::Left ? TurnSignalLampSide::Right
                                            : TurnSignalLampSide::Left);
        // A retained dark phase must not turn a direction change into hazards.
        // Keep an actually illuminated peer (hazards), and respect newer data.
        if (!opposite.illuminated && opposite.received_at <= received_at) {
            opposite.last_illuminated_at.reset();
        }
    }
    return true;
}

bool TelemetryStore::mark_turn_signal_lamp_unavailable(
    TurnSignalLampSide side,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    auto& slot = turn_signal_slot(side);
    if (slot.status != SignalStatus::NeverReceived &&
        received_at < slot.received_at) {
        return false;
    }

    slot.illuminated = false;
    slot.source_timestamp_microseconds = source_timestamp_microseconds;
    slot.received_at = received_at;
    slot.last_illuminated_at.reset();
    slot.status = SignalStatus::Unavailable;
    return true;
}

TurnSignalTelemetry TelemetryStore::turn_signals(TelemetryTime now) const
    noexcept {
    const auto left_status = turn_signal_status(left_turn_signal_, now);
    const auto right_status = turn_signal_status(right_turn_signal_, now);
    const bool left_active =
        turn_signal_active(left_turn_signal_, left_status, now);
    const bool right_active =
        turn_signal_active(right_turn_signal_, right_status, now);
    const bool left_illuminated =
        left_status == SignalStatus::Fresh && left_turn_signal_.illuminated;
    const bool right_illuminated =
        right_status == SignalStatus::Fresh && right_turn_signal_.illuminated;

    return {
        domain::TurnSignals::from_lamps(left_active, right_active),
        (left_active && left_illuminated) ||
            (right_active && right_illuminated),
        left_status,
        right_status,
        left_turn_signal_.source_timestamp_microseconds,
        right_turn_signal_.source_timestamp_microseconds,
    };
}

bool TelemetryStore::update_blind_spot(
    BlindSpotSide side,
    bool occupied,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    auto& slot = blind_spot_slot(side);
    if (slot.status != SignalStatus::NeverReceived &&
        received_at < slot.received_at) {
        return false;
    }

    slot.value = occupied;
    slot.source_timestamp_microseconds = source_timestamp_microseconds;
    slot.received_at = received_at;
    slot.status = SignalStatus::Fresh;
    return true;
}

bool TelemetryStore::mark_blind_spot_unavailable(
    BlindSpotSide side,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    auto& slot = blind_spot_slot(side);
    if (slot.status != SignalStatus::NeverReceived &&
        received_at < slot.received_at) {
        return false;
    }

    slot.value = false;
    slot.source_timestamp_microseconds = source_timestamp_microseconds;
    slot.received_at = received_at;
    slot.status = SignalStatus::Unavailable;
    return true;
}

BlindSpotTelemetry TelemetryStore::blind_spots(TelemetryTime now) const
    noexcept {
    const auto left_status = status_at(
        left_blind_spot_.status,
        left_blind_spot_.received_at,
        policy_.blind_spot_timeout,
        now);
    const auto right_status = status_at(
        right_blind_spot_.status,
        right_blind_spot_.received_at,
        policy_.blind_spot_timeout,
        now);
    const bool left_occupied =
        left_status == SignalStatus::Fresh && left_blind_spot_.value;
    const bool right_occupied =
        right_status == SignalStatus::Fresh && right_blind_spot_.value;

    return {
        domain::BlindSpots::from_occupancy(left_occupied, right_occupied),
        left_status,
        right_status,
        left_blind_spot_.source_timestamp_microseconds,
        right_blind_spot_.source_timestamp_microseconds,
    };
}

bool TelemetryStore::update_autopilot_state(
    domain::AutopilotState state,
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    if (autopilot_status_ != SignalStatus::NeverReceived &&
        received_at < autopilot_received_at_) {
        return false;
    }

    autopilot_state_ = state;
    autopilot_source_timestamp_ = source_timestamp_microseconds;
    autopilot_received_at_ = received_at;
    autopilot_status_ = SignalStatus::Fresh;
    return true;
}

bool TelemetryStore::mark_autopilot_unavailable(
    std::uint32_t source_timestamp_microseconds,
    TelemetryTime received_at) noexcept {
    if (autopilot_status_ != SignalStatus::NeverReceived &&
        received_at < autopilot_received_at_) {
        return false;
    }

    autopilot_state_.reset();
    autopilot_source_timestamp_ = source_timestamp_microseconds;
    autopilot_received_at_ = received_at;
    autopilot_status_ = SignalStatus::Unavailable;
    return true;
}

AutopilotTelemetry TelemetryStore::autopilot(TelemetryTime now) const
    noexcept {
    const auto status = status_at(
        autopilot_status_,
        autopilot_received_at_,
        policy_.autopilot_timeout,
        now);
    return {
        status,
        status == SignalStatus::Fresh ? autopilot_state_ : std::nullopt,
        autopilot_source_timestamp_,
        autopilot_received_at_,
    };
}

bool TelemetryStore::accepts_vehicle_speed(
    TelemetryTime received_at) const noexcept {
    return vehicle_speed_status_ == SignalStatus::NeverReceived ||
           received_at >= vehicle_speed_received_at_;
}

TelemetryStore::TurnSignalLampSlot& TelemetryStore::turn_signal_slot(
    TurnSignalLampSide side) noexcept {
    return side == TurnSignalLampSide::Left ? left_turn_signal_
                                            : right_turn_signal_;
}

const TelemetryStore::TurnSignalLampSlot& TelemetryStore::turn_signal_slot(
    TurnSignalLampSide side) const noexcept {
    return side == TurnSignalLampSide::Left ? left_turn_signal_
                                            : right_turn_signal_;
}

SignalStatus TelemetryStore::turn_signal_status(
    const TurnSignalLampSlot& slot,
    TelemetryTime now) const noexcept {
    if (slot.status == SignalStatus::NeverReceived ||
        slot.status == SignalStatus::Unavailable) {
        return slot.status;
    }

    const auto timeout = policy_.turn_signal_timeout < TelemetryTime::zero()
                             ? TelemetryTime::zero()
                             : policy_.turn_signal_timeout;
    if (now < slot.received_at || (now - slot.received_at) <= timeout) {
        return SignalStatus::Fresh;
    }
    return SignalStatus::Stale;
}

bool TelemetryStore::turn_signal_active(
    const TurnSignalLampSlot& slot,
    SignalStatus status,
    TelemetryTime now) const noexcept {
    if (status != SignalStatus::Fresh ||
        !slot.last_illuminated_at.has_value()) {
        return false;
    }

    const auto hold = policy_.turn_signal_hold < TelemetryTime::zero()
                          ? TelemetryTime::zero()
                          : policy_.turn_signal_hold;
    return now < *slot.last_illuminated_at ||
           (now - *slot.last_illuminated_at) <= hold;
}

TelemetryStore::BooleanSignalSlot& TelemetryStore::blind_spot_slot(
    BlindSpotSide side) noexcept {
    return side == BlindSpotSide::Left ? left_blind_spot_
                                       : right_blind_spot_;
}

const TelemetryStore::BooleanSignalSlot& TelemetryStore::blind_spot_slot(
    BlindSpotSide side) const noexcept {
    return side == BlindSpotSide::Left ? left_blind_spot_
                                       : right_blind_spot_;
}

SignalStatus TelemetryStore::status_at(
    SignalStatus status,
    TelemetryTime received_at,
    TelemetryTime timeout,
    TelemetryTime now) noexcept {
    if (status == SignalStatus::NeverReceived ||
        status == SignalStatus::Unavailable) {
        return status;
    }

    const auto bounded_timeout =
        timeout < TelemetryTime::zero() ? TelemetryTime::zero() : timeout;
    if (now < received_at || (now - received_at) <= bounded_timeout) {
        return SignalStatus::Fresh;
    }
    return SignalStatus::Stale;
}

}  // namespace tesla_headup::application
