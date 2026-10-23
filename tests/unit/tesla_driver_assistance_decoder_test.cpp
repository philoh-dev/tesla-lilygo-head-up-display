#include "test.hpp"

#include "tesla_headup/adapters/tesla_decoder/tesla_driver_assistance_decoder.hpp"

#include <array>
#include <cstdint>

namespace {

using tesla_headup::dashkit::protocol::CanFrameView;
using tesla_headup::domain::AutopilotState;
using tesla_headup::tesla::DriverAssistanceDecodeStatus;
using tesla_headup::tesla::TeslaSignalValidity;
using tesla_headup::tesla::decode_driver_assistance;

CanFrameView das_status_frame(
    std::uint8_t packed_state,
    std::array<std::uint8_t, 8>& data,
    std::uint32_t timestamp = 123,
    std::uint8_t raw_speed_limit = 0) {
    data = {packed_state, raw_speed_limit, 0, 0, 0, 0, 0, 0};
    return {timestamp, 0, 921, data};
}

TEST_CASE("decodes the fused speed limit in five-unit increments") {
    std::array<std::uint8_t, 8> data{};

    const auto result = decode_driver_assistance(
        das_status_frame(0, data, 321, 10));

    CHECK_EQ(result.speed_limit.validity, TeslaSignalValidity::Valid);
    CHECK(result.speed_limit.value.has_value());
    CHECK_EQ(result.speed_limit.value->display_value(), 50U);
    CHECK_EQ(result.speed_limit.raw_value, 10U);
}

TEST_CASE("unknown and absent fused speed limits are unavailable") {
    std::array<std::uint8_t, 8> data{};

    for (const std::uint8_t raw : {0U, 31U}) {
        const auto result = decode_driver_assistance(
            das_status_frame(0, data, 123, raw));
        CHECK_EQ(
            result.speed_limit.validity,
            TeslaSignalValidity::Unavailable);
        CHECK(!result.speed_limit.value.has_value());
    }
}

TEST_CASE("decodes both blind-spot warning levels") {
    std::array<std::uint8_t, 8> data{};
    const auto result = decode_driver_assistance(
        das_status_frame(0x90, data, 456));

    CHECK(result.decoded());
    CHECK_EQ(result.left_blind_spot.validity, TeslaSignalValidity::Valid);
    CHECK(result.left_blind_spot.occupied);
    CHECK_EQ(result.left_blind_spot.warning_level, 1U);
    CHECK_EQ(result.right_blind_spot.validity, TeslaSignalValidity::Valid);
    CHECK(result.right_blind_spot.occupied);
    CHECK_EQ(result.right_blind_spot.warning_level, 2U);
    CHECK_EQ(result.source_timestamp_microseconds, 456U);
}

TEST_CASE("blind-spot clear and unavailable values remain distinct") {
    std::array<std::uint8_t, 8> data{};
    const auto result = decode_driver_assistance(
        das_status_frame(0xC0, data));

    CHECK_EQ(result.left_blind_spot.validity, TeslaSignalValidity::Valid);
    CHECK(!result.left_blind_spot.occupied);
    CHECK_EQ(
        result.right_blind_spot.validity,
        TeslaSignalValidity::Unavailable);
    CHECK(!result.right_blind_spot.occupied);
}

TEST_CASE("decodes every active Tesla Autopilot mode") {
    const std::array expected{
        AutopilotState::ActiveNominal,
        AutopilotState::ActiveRestricted,
        AutopilotState::ActiveNavigation,
        AutopilotState::ActiveFullSelfDriving,
    };
    std::array<std::uint8_t, 8> data{};

    for (std::uint8_t raw = 3; raw <= 6; ++raw) {
        const auto result = decode_driver_assistance(
            das_status_frame(raw, data));
        CHECK_EQ(result.autopilot.validity, TeslaSignalValidity::Valid);
        CHECK_EQ(result.autopilot.state, expected.at(raw - 3U));
    }
}

TEST_CASE("decodes inactive and transitional Autopilot states") {
    const std::array expected{
        AutopilotState::Disabled,
        AutopilotState::Unavailable,
        AutopilotState::Available,
        AutopilotState::Aborting,
        AutopilotState::Aborted,
    };
    const std::array<std::uint8_t, 5> raw_values{0, 1, 2, 8, 9};
    std::array<std::uint8_t, 8> data{};

    for (std::size_t index = 0; index < raw_values.size(); ++index) {
        const auto result = decode_driver_assistance(
            das_status_frame(raw_values.at(index), data));
        CHECK_EQ(result.autopilot.validity, TeslaSignalValidity::Valid);
        CHECK_EQ(result.autopilot.state, expected.at(index));
    }
}

TEST_CASE("Autopilot fault, SNA, and reserved states are explicit") {
    std::array<std::uint8_t, 8> data{};

    CHECK_EQ(
        decode_driver_assistance(das_status_frame(14, data))
            .autopilot.validity,
        TeslaSignalValidity::Fault);
    CHECK_EQ(
        decode_driver_assistance(das_status_frame(15, data))
            .autopilot.validity,
        TeslaSignalValidity::Unavailable);
    CHECK_EQ(
        decode_driver_assistance(das_status_frame(7, data))
            .autopilot.validity,
        TeslaSignalValidity::Invalid);
    CHECK_EQ(
        decode_driver_assistance(das_status_frame(10, data))
            .autopilot.validity,
        TeslaSignalValidity::Invalid);
}

TEST_CASE("driver-assistance decoder ignores unrelated CAN traffic") {
    const std::array<std::uint8_t, 8> data{};

    CHECK_EQ(
        decode_driver_assistance(CanFrameView{1, 1, 921, data}).status,
        DriverAssistanceDecodeStatus::Ignored);
    CHECK_EQ(
        decode_driver_assistance(CanFrameView{1, 0, 920, data}).status,
        DriverAssistanceDecodeStatus::Ignored);
}

TEST_CASE("driver-assistance decoder requires the exact DAS status DLC") {
    const std::array<std::uint8_t, 7> short_data{};

    CHECK_EQ(
        decode_driver_assistance(CanFrameView{1, 0, 921, short_data}).status,
        DriverAssistanceDecodeStatus::InvalidFrame);
}

static_assert(noexcept(decode_driver_assistance(CanFrameView{})));
static_assert(tesla_headup::domain::autopilot_active(
    AutopilotState::ActiveFullSelfDriving));
static_assert(!tesla_headup::domain::autopilot_active(AutopilotState::Available));

}  // namespace
