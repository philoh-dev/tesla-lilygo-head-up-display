#include "test.hpp"

#include "tesla_headup/application/hud_runtime/display_self_test_source.hpp"

#include <array>
#include <chrono>

namespace {

using namespace std::chrono_literals;
using tesla_headup::application::DisplaySelfTestPhase;
using tesla_headup::application::SignalStatus;

tesla_headup::application::DisplaySelfTestSource make_source(
    tesla_headup::application::TelemetryStore& telemetry) {
    return tesla_headup::application::DisplaySelfTestSource{
        telemetry,
        tesla_headup::application::DisplaySelfTestPolicy{
            .update_interval = 1ms,
            .blink_interval = 100s,
        }};
}

TEST_CASE("display self-test follows the staged drive timeline") {
    tesla_headup::application::TelemetryStore telemetry;
    auto source = make_source(telemetry);

    CHECK(source.tick(10s));
    constexpr std::array expected{
        DisplaySelfTestPhase::Park,
        DisplaySelfTestPhase::Accelerating,
        DisplaySelfTestPhase::Cruising,
        DisplaySelfTestPhase::LeftSignal,
        DisplaySelfTestPhase::LeftBlindSpot,
        DisplaySelfTestPhase::LeftSignal,
        DisplaySelfTestPhase::Autopilot,
        DisplaySelfTestPhase::Decelerating,
        DisplaySelfTestPhase::ReverseWait,
        DisplaySelfTestPhase::ReverseAccelerating,
        DisplaySelfTestPhase::FeatureSweep,
        DisplaySelfTestPhase::AllFeatures,
    };
    constexpr std::array offsets{
        0s, 2s, 9s, 14s, 15s, 18s, 21s, 26s, 41s, 42s, 45s, 73s};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CHECK_EQ(source.phase(10s + offsets[index]), expected[index]);
    }
}

TEST_CASE("self-test starts in Park with every other feature inactive") {
    tesla_headup::application::TelemetryStore telemetry;
    auto source = make_source(telemetry);

    CHECK(source.tick(0ms));
    CHECK_EQ(telemetry.vehicle_speed(0ms).status, SignalStatus::Unavailable);
    CHECK_EQ(
        *telemetry.selected_gear(0ms).current_value,
        tesla_headup::domain::SelectedGear::Park);
    CHECK_EQ(
        telemetry.autopilot(0ms).current_value,
        tesla_headup::domain::AutopilotState::Disabled);
    CHECK_EQ(
        telemetry.turn_signals(0ms).signals,
        tesla_headup::domain::TurnSignals::off());
    CHECK_EQ(
        telemetry.blind_spots(0ms).current_value,
        tesla_headup::domain::BlindSpots::clear());
    CHECK_EQ(telemetry.speed_limit(0ms).status, SignalStatus::Unavailable);
}

TEST_CASE("self-test accelerates and decelerates over the requested times") {
    tesla_headup::application::TelemetryStore telemetry;
    auto source = make_source(telemetry);
    CHECK(source.tick(0ms));

    CHECK(source.tick(2s));
    CHECK_EQ(
        telemetry.vehicle_speed(2s).current_value->kilometres_per_hour(), 0);
    CHECK_EQ(
        *telemetry.selected_gear(2s).current_value,
        tesla_headup::domain::SelectedGear::Drive);
    CHECK_EQ(telemetry.speed_limit(2s).current_value->display_value(), 50U);

    CHECK(source.tick(5'500ms));
    CHECK_EQ(
        telemetry.vehicle_speed(5'500ms)
            .current_value->kilometres_per_hour(),
        65);
    CHECK(source.tick(9s));
    CHECK_EQ(
        telemetry.vehicle_speed(9s).current_value->kilometres_per_hour(),
        130);

    CHECK(source.tick(33'500ms));
    CHECK_EQ(
        telemetry.vehicle_speed(33'500ms)
            .current_value->kilometres_per_hour(),
        65);
    CHECK(source.tick(41s));
    CHECK_EQ(
        telemetry.vehicle_speed(41s).current_value->kilometres_per_hour(), 0);
}

TEST_CASE("self-test simulates a blocked left lane change") {
    tesla_headup::application::TelemetryStore telemetry;
    auto source = make_source(telemetry);
    CHECK(source.tick(0ms));

    CHECK(source.tick(14s));
    CHECK(telemetry.turn_signals(14s).signals.left_active());
    CHECK(!telemetry.turn_signals(14s).signals.right_active());

    CHECK(source.tick(15s));
    CHECK(telemetry.turn_signals(15s).signals.left_active());
    CHECK(!telemetry.turn_signals(15s).signals.right_active());
    CHECK(telemetry.blind_spots(15s).current_value.left_occupied());
    CHECK(!telemetry.blind_spots(15s).current_value.right_occupied());

    CHECK(source.tick(18s));
    CHECK(telemetry.turn_signals(18s).signals.left_active());
    CHECK(!telemetry.turn_signals(18s).signals.right_active());
    CHECK(!telemetry.blind_spots(18s).current_value.left_occupied());

    CHECK(source.tick(21s));
    CHECK_EQ(
        telemetry.turn_signals(21s).signals,
        tesla_headup::domain::TurnSignals::off());
    CHECK(telemetry.autopilot(21s).active());

    CHECK(source.tick(26s));
    CHECK(!telemetry.autopilot(26s).active());
}

TEST_CASE("self-test accelerates in Reverse after stopping") {
    tesla_headup::application::TelemetryStore telemetry;
    auto source = make_source(telemetry);
    CHECK(source.tick(0ms));

    CHECK(source.tick(42s));
    CHECK_EQ(
        *telemetry.selected_gear(42s).current_value,
        tesla_headup::domain::SelectedGear::Reverse);
    CHECK_EQ(
        telemetry.vehicle_speed(42s).current_value->kilometres_per_hour(), 0);

    CHECK(source.tick(43'500ms));
    CHECK_EQ(
        telemetry.vehicle_speed(43'500ms)
            .current_value->kilometres_per_hour(),
        8);
    CHECK(source.tick(44'900ms));
    CHECK_EQ(
        telemetry.vehicle_speed(44'900ms)
            .current_value->kilometres_per_hour(),
        15);
    CHECK(source.tick(45s));
    CHECK_EQ(
        telemetry.vehicle_speed(45s).current_value->kilometres_per_hour(),
        88);
}

TEST_CASE("feature sweep activates and clears every visual independently") {
    tesla_headup::application::TelemetryStore telemetry;
    auto source = make_source(telemetry);
    CHECK(source.tick(0ms));

    CHECK(source.tick(45s));
    CHECK_EQ(
        telemetry.vehicle_speed(45s).current_value->kilometres_per_hour(),
        88);
    CHECK(source.tick(46'500ms));
    CHECK_EQ(
        telemetry.vehicle_speed(46'500ms).status,
        SignalStatus::Unavailable);

    CHECK(source.tick(47'500ms));
    CHECK_EQ(
        *telemetry.selected_gear(47'500ms).current_value,
        tesla_headup::domain::SelectedGear::Drive);
    CHECK(source.tick(49s));
    CHECK_EQ(
        telemetry.selected_gear(49s).status, SignalStatus::Unavailable);

    CHECK(source.tick(50s));
    CHECK(telemetry.turn_signals(50s).signals.left_active());
    CHECK(source.tick(51'500ms));
    CHECK_EQ(
        telemetry.turn_signals(51'500ms).signals,
        tesla_headup::domain::TurnSignals::off());

    CHECK(source.tick(52'500ms));
    CHECK(telemetry.turn_signals(52'500ms).signals.right_active());
    CHECK(source.tick(54s));
    CHECK_EQ(
        telemetry.turn_signals(54s).signals,
        tesla_headup::domain::TurnSignals::off());

    CHECK(source.tick(55s));
    CHECK(telemetry.turn_signals(55s).signals.left_active());
    CHECK(telemetry.turn_signals(55s).signals.right_active());
    CHECK(source.tick(56'500ms));
    CHECK_EQ(
        telemetry.turn_signals(56'500ms).signals,
        tesla_headup::domain::TurnSignals::off());

    CHECK(source.tick(57'500ms));
    CHECK(telemetry.blind_spots(57'500ms).current_value.left_occupied());
    CHECK(source.tick(59s));
    CHECK_EQ(
        telemetry.blind_spots(59s).current_value,
        tesla_headup::domain::BlindSpots::clear());

    CHECK(source.tick(60s));
    CHECK(telemetry.blind_spots(60s).current_value.right_occupied());
    CHECK(source.tick(61'500ms));
    CHECK_EQ(
        telemetry.blind_spots(61'500ms).current_value,
        tesla_headup::domain::BlindSpots::clear());

    CHECK(source.tick(62'500ms));
    CHECK(telemetry.autopilot(62'500ms).active());
    CHECK(source.tick(64s));
    CHECK(!telemetry.autopilot(64s).active());

    CHECK(source.tick(65s));
    CHECK_EQ(telemetry.speed_limit(65s).current_value->display_value(), 50U);
    CHECK(source.tick(66'500ms));
    CHECK_EQ(telemetry.speed_limit(66'500ms).status,
             SignalStatus::Unavailable);
}

TEST_CASE("self-test finishes with all compatible features active") {
    tesla_headup::application::TelemetryStore telemetry;
    auto source = make_source(telemetry);
    CHECK(source.tick(0ms));
    CHECK(source.tick(73s));

    CHECK_EQ(
        telemetry.vehicle_speed(73s).current_value->kilometres_per_hour(),
        130);
    CHECK_EQ(
        *telemetry.selected_gear(73s).current_value,
        tesla_headup::domain::SelectedGear::Drive);
    CHECK(telemetry.turn_signals(73s).signals.left_active());
    CHECK(telemetry.turn_signals(73s).signals.right_active());
    CHECK(telemetry.blind_spots(73s).current_value.left_occupied());
    CHECK(telemetry.blind_spots(73s).current_value.right_occupied());
    CHECK(telemetry.autopilot(73s).active());
    CHECK_EQ(telemetry.speed_limit(73s).current_value->display_value(), 50U);
    CHECK_EQ(telemetry.power_flow(73s)->permille(), 250);
}

TEST_CASE("display self-test blinks active turn lamps without stale data") {
    tesla_headup::application::TelemetryStore telemetry;
    tesla_headup::application::DisplaySelfTestSource source{telemetry};
    CHECK(source.tick(0ms));

    CHECK(source.tick(14s));
    CHECK(telemetry.turn_signals(14s).signals.left_active());
    CHECK(telemetry.turn_signals(14s).illuminated);
    CHECK(source.tick(14'500ms));
    CHECK(telemetry.turn_signals(14'500ms).signals.left_active());
    CHECK(!telemetry.turn_signals(14'500ms).illuminated);
    CHECK(telemetry.vehicle_speed(14'500ms).fresh());
}

TEST_CASE("display self-test throttles updates and restarts after clock reset") {
    tesla_headup::application::TelemetryStore telemetry;
    tesla_headup::application::DisplaySelfTestSource source{telemetry};

    CHECK(source.tick(1s));
    CHECK(!source.tick(1'099ms));
    CHECK(source.tick(1'100ms));
    CHECK(source.tick(500ms));
    CHECK_EQ(source.phase(500ms), DisplaySelfTestPhase::Park);
}

}  // namespace
