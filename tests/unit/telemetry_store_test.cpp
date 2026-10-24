#include "test.hpp"

#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <chrono>
#include <utility>

namespace {

using namespace std::chrono_literals;
using tesla_headup::application::SignalStatus;
using tesla_headup::application::TelemetryFreshnessPolicy;
using tesla_headup::application::TelemetryStore;
using tesla_headup::application::BlindSpotSide;
using tesla_headup::application::TurnSignalLampSide;
using tesla_headup::domain::AutopilotState;
using tesla_headup::domain::BlindSpots;
using tesla_headup::domain::TurnSignals;
using tesla_headup::domain::VehicleSpeed;

VehicleSpeed speed(std::uint16_t kilometres_per_hour) {
    return *VehicleSpeed::from_kilometres_per_hour(kilometres_per_hour);
}

TEST_CASE("speed is missing before the first observation") {
    const TelemetryStore store;

    const auto snapshot = store.vehicle_speed(10s);

    CHECK_EQ(snapshot.status, SignalStatus::NeverReceived);
    CHECK(!snapshot.current_value.has_value());
    CHECK(!snapshot.fresh());
}

TEST_CASE("a speed observation is fresh and retains both timestamps") {
    TelemetryStore store;

    CHECK(store.update_vehicle_speed(speed(135), 4'294'967'000U, 10s));
    const auto snapshot = store.vehicle_speed(10s);

    CHECK(snapshot.fresh());
    CHECK_EQ(snapshot.current_value->kilometres_per_hour(), 135);
    CHECK_EQ(snapshot.source_timestamp_microseconds, 4'294'967'000U);
    CHECK_EQ(snapshot.received_at, 10s);
}

TEST_CASE("speed remains fresh exactly through the configured timeout") {
    TelemetryFreshnessPolicy policy;
    policy.vehicle_speed_timeout = 500ms;
    TelemetryStore store{policy};
    CHECK(store.update_vehicle_speed(speed(42), 1, 1s));

    CHECK(store.vehicle_speed(1499ms).fresh());
    CHECK(store.vehicle_speed(1500ms).fresh());
}

TEST_CASE("expired speed is stale and cannot be presented as current") {
    TelemetryStore store;
    CHECK(store.update_vehicle_speed(speed(88), 1, 1s));

    const auto snapshot = store.vehicle_speed(1501ms);

    CHECK_EQ(snapshot.status, SignalStatus::Stale);
    CHECK(!snapshot.current_value.has_value());
    CHECK_EQ(snapshot.source_timestamp_microseconds, 1U);
    CHECK_EQ(snapshot.received_at, 1s);
}

TEST_CASE("a new observation restores stale speed to fresh") {
    TelemetryStore store;
    CHECK(store.update_vehicle_speed(speed(10), 1, 0ms));
    CHECK_EQ(store.vehicle_speed(1s).status, SignalStatus::Stale);

    CHECK(store.update_vehicle_speed(speed(11), 2, 1s));

    const auto snapshot = store.vehicle_speed(1s);
    CHECK(snapshot.fresh());
    CHECK_EQ(snapshot.current_value->kilometres_per_hour(), 11);
}

TEST_CASE("an unavailable observation immediately clears current speed") {
    TelemetryStore store;
    CHECK(store.update_vehicle_speed(speed(100), 10, 1s));

    CHECK(store.mark_vehicle_speed_unavailable(11, 1100ms));
    const auto snapshot = store.vehicle_speed(1100ms);

    CHECK_EQ(snapshot.status, SignalStatus::Unavailable);
    CHECK(!snapshot.current_value.has_value());
    CHECK_EQ(snapshot.source_timestamp_microseconds, 11U);
    CHECK_EQ(snapshot.received_at, 1100ms);
}

TEST_CASE("unavailable state does not turn into stale state over time") {
    TelemetryStore store;
    CHECK(store.mark_vehicle_speed_unavailable(5, 1s));

    const auto snapshot = store.vehicle_speed(1h);

    CHECK_EQ(snapshot.status, SignalStatus::Unavailable);
    CHECK(!snapshot.current_value.has_value());
}

TEST_CASE("out-of-order speed updates are rejected by local receipt time") {
    TelemetryStore store;
    CHECK(store.update_vehicle_speed(speed(80), 200, 2s));

    CHECK(!store.update_vehicle_speed(speed(20), 100, 1999ms));

    const auto snapshot = store.vehicle_speed(2s);
    CHECK_EQ(snapshot.current_value->kilometres_per_hour(), 80);
    CHECK_EQ(snapshot.source_timestamp_microseconds, 200U);
}

TEST_CASE("out-of-order unavailable updates cannot erase newer speed") {
    TelemetryStore store;
    CHECK(store.update_vehicle_speed(speed(80), 200, 2s));

    CHECK(!store.mark_vehicle_speed_unavailable(100, 1999ms));

    CHECK(store.vehicle_speed(2s).fresh());
}

TEST_CASE("updates in the same local clock tick use arrival order") {
    TelemetryStore store;
    CHECK(store.update_vehicle_speed(speed(80), 100, 2s));

    CHECK(store.update_vehicle_speed(speed(81), 101, 2s));

    CHECK_EQ(
        store.vehicle_speed(2s).current_value->kilometres_per_hour(),
        81);
}

TEST_CASE("a clock value before receipt does not falsely expire speed") {
    TelemetryStore store;
    CHECK(store.update_vehicle_speed(speed(50), 1, 2s));

    CHECK(store.vehicle_speed(1s).fresh());
}

TEST_CASE("a custom freshness timeout is honored") {
    TelemetryFreshnessPolicy policy;
    policy.vehicle_speed_timeout = 2s;
    TelemetryStore store{policy};
    CHECK(store.update_vehicle_speed(speed(50), 1, 1s));

    CHECK(store.vehicle_speed(3s).fresh());
    CHECK_EQ(store.vehicle_speed(3001ms).status, SignalStatus::Stale);
}

TEST_CASE("negative freshness timeouts are safely treated as zero") {
    TelemetryFreshnessPolicy policy;
    policy.vehicle_speed_timeout = -1ms;
    TelemetryStore store{policy};
    CHECK(store.update_vehicle_speed(speed(50), 1, 1s));

    CHECK(store.vehicle_speed(1s).fresh());
    CHECK_EQ(store.vehicle_speed(1001ms).status, SignalStatus::Stale);
}

TEST_CASE("turn signals are off before either lamp is observed") {
    const TelemetryStore store;

    const auto snapshot = store.turn_signals(1s);

    CHECK_EQ(snapshot.signals, TurnSignals::off());
    CHECK(!snapshot.illuminated);
    CHECK_EQ(snapshot.left_status, SignalStatus::NeverReceived);
    CHECK_EQ(snapshot.right_status, SignalStatus::NeverReceived);
}

TEST_CASE("a dark lamp phase retains turn intent for one second") {
    TelemetryStore store;
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Left, true, 10, 100ms));
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Left, false, 11, 500ms));

    const auto dark = store.turn_signals(500ms);
    CHECK_EQ(dark.signals, TurnSignals::left());
    CHECK(!dark.illuminated);
    CHECK_EQ(dark.left_source_timestamp_microseconds, 11U);

    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Left, false, 12, 1101ms));
    CHECK_EQ(store.turn_signals(1101ms).signals, TurnSignals::off());
}

TEST_CASE("both recently illuminated lamps produce a hazard state") {
    TelemetryStore store;
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Left, true, 1, 100ms));
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Right, true, 2, 100ms));

    const auto snapshot = store.turn_signals(100ms);
    CHECK_EQ(snapshot.signals, TurnSignals::hazard());
    CHECK(snapshot.illuminated);
}

TEST_CASE("stale turn lamp data is cleared before its intent hold expires") {
    TelemetryStore store;
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Right, true, 1, 0ms));

    const auto snapshot = store.turn_signals(501ms);

    CHECK_EQ(snapshot.right_status, SignalStatus::Stale);
    CHECK_EQ(snapshot.signals, TurnSignals::off());
    CHECK(!snapshot.illuminated);
}

TEST_CASE("unavailable turn lamp data immediately clears that side") {
    TelemetryStore store;
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Left, true, 1, 100ms));
    CHECK(store.mark_turn_signal_lamp_unavailable(
        TurnSignalLampSide::Left, 2, 101ms));

    const auto snapshot = store.turn_signals(101ms);
    CHECK_EQ(snapshot.left_status, SignalStatus::Unavailable);
    CHECK_EQ(snapshot.signals, TurnSignals::off());
}

TEST_CASE("turn lamp ordering is tracked independently per side") {
    TelemetryStore store;
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Left, true, 20, 2s));

    CHECK(!store.update_turn_signal_lamp(
        TurnSignalLampSide::Left, false, 10, 1999ms));
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Right, true, 10, 1999ms));

    CHECK_EQ(store.turn_signals(2s).signals, TurnSignals::hazard());
}

TEST_CASE("older opposite illumination cannot clear newer dark turn intent") {
    TelemetryStore store;
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Left, true, 1, 100ms));
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Left, false, 2, 200ms));
    CHECK(store.update_turn_signal_lamp(
        TurnSignalLampSide::Right, true, 3, 150ms));
    CHECK_EQ(store.turn_signals(200ms).signals, TurnSignals::hazard());
}

TEST_CASE("blind spots are clear before observations arrive") {
    const TelemetryStore store;

    const auto snapshot = store.blind_spots(1s);

    CHECK_EQ(snapshot.current_value, BlindSpots::clear());
    CHECK_EQ(snapshot.left_status, SignalStatus::NeverReceived);
    CHECK_EQ(snapshot.right_status, SignalStatus::NeverReceived);
}

TEST_CASE("blind-spot sides update and expire independently") {
    TelemetryStore store;
    CHECK(store.update_blind_spot(BlindSpotSide::Left, true, 10, 0ms));
    CHECK(store.update_blind_spot(BlindSpotSide::Right, true, 20, 400ms));

    const auto snapshot = store.blind_spots(501ms);

    CHECK_EQ(snapshot.left_status, SignalStatus::Stale);
    CHECK_EQ(snapshot.right_status, SignalStatus::Fresh);
    CHECK(!snapshot.current_value.left_occupied());
    CHECK(snapshot.current_value.right_occupied());
}

TEST_CASE("unavailable blind-spot data fails clear") {
    TelemetryStore store;
    CHECK(store.update_blind_spot(BlindSpotSide::Left, true, 1, 100ms));
    CHECK(store.mark_blind_spot_unavailable(
        BlindSpotSide::Left, 2, 101ms));

    const auto snapshot = store.blind_spots(101ms);
    CHECK_EQ(snapshot.left_status, SignalStatus::Unavailable);
    CHECK_EQ(snapshot.current_value, BlindSpots::clear());
    CHECK_EQ(snapshot.left_source_timestamp_microseconds, 2U);
}

TEST_CASE("out-of-order blind-spot data cannot overwrite a side") {
    TelemetryStore store;
    CHECK(store.update_blind_spot(BlindSpotSide::Right, true, 2, 2s));

    CHECK(!store.update_blind_spot(
        BlindSpotSide::Right, false, 1, 1999ms));

    CHECK(store.blind_spots(2s).current_value.right_occupied());
}

TEST_CASE("Autopilot telemetry preserves detailed active states") {
    TelemetryStore store;
    CHECK(store.update_autopilot_state(
        AutopilotState::ActiveFullSelfDriving, 42, 100ms));

    const auto snapshot = store.autopilot(100ms);
    CHECK(snapshot.fresh());
    CHECK(snapshot.active());
    CHECK_EQ(*snapshot.current_value,
             AutopilotState::ActiveFullSelfDriving);
    CHECK_EQ(snapshot.source_timestamp_microseconds, 42U);
}

TEST_CASE("available Autopilot is fresh but not active") {
    TelemetryStore store;
    CHECK(store.update_autopilot_state(
        AutopilotState::Available, 1, 100ms));

    const auto snapshot = store.autopilot(100ms);
    CHECK(snapshot.fresh());
    CHECK(!snapshot.active());
}

TEST_CASE("stale and unavailable Autopilot telemetry fails inactive") {
    TelemetryStore store;
    CHECK(store.update_autopilot_state(
        AutopilotState::ActiveNominal, 1, 0ms));
    CHECK(store.autopilot(501ms).active());
    CHECK(store.autopilot(1500ms).active());
    CHECK_EQ(store.autopilot(1501ms).status, SignalStatus::Stale);
    CHECK(!store.autopilot(1501ms).active());

    CHECK(store.mark_autopilot_unavailable(2, 1600ms));
    CHECK_EQ(store.autopilot(1600ms).status, SignalStatus::Unavailable);
    CHECK(!store.autopilot(1600ms).current_value.has_value());
}

TEST_CASE("Autopilot freshness can be overridden independently") {
    tesla_headup::application::TelemetryFreshnessPolicy policy;
    policy.autopilot_timeout = 250ms;
    TelemetryStore store{policy};
    CHECK(store.update_autopilot_state(AutopilotState::ActiveNominal, 1, 100ms));
    CHECK(store.autopilot(350ms).active());
    CHECK_EQ(store.autopilot(351ms).status, SignalStatus::Stale);
}

TEST_CASE("out-of-order Autopilot telemetry is rejected") {
    TelemetryStore store;
    CHECK(store.update_autopilot_state(
        AutopilotState::ActiveNominal, 2, 2s));

    CHECK(!store.update_autopilot_state(
        AutopilotState::Disabled, 1, 1999ms));

    CHECK(store.autopilot(2s).active());
}

static_assert(noexcept(
    std::declval<const TelemetryStore&>().vehicle_speed(
        std::chrono::milliseconds{})));

}  // namespace
