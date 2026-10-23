#include "test.hpp"

#include "tesla_headup/adapters/tesla_decoder/tesla_speed_decoder.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace {

using tesla_headup::dashkit::protocol::CanFrameView;
using tesla_headup::tesla::SpeedDecodeStatus;
using tesla_headup::tesla::decode_vehicle_speed;

CanFrameView speed_frame(
    const std::array<std::uint8_t, 8>& data,
    std::uint32_t timestamp = 123'456) {
    return {timestamp, 0, 599, data};
}

TEST_CASE("decodes Tesla displayed speed for the metric vehicle profile") {
    const std::array<std::uint8_t, 8> data{
        0, 0xC0, 0x88, 135, 1, 0, 0, 0};

    const auto result = decode_vehicle_speed(speed_frame(data, 987'654));

    CHECK(result.decoded());
    CHECK(result.speed.has_value());
    CHECK_EQ(result.speed->kilometres_per_hour(), 135);
    CHECK_EQ(result.source_timestamp_microseconds, 987'654U);
}

TEST_CASE("metric 16 kmh cannot be misinterpreted as 16 mph") {
    const std::array<std::uint8_t, 8> data{
        0, 0xC0, 0x2B, 16, 0, 0, 0, 0};

    const auto result = decode_vehicle_speed(speed_frame(data));

    CHECK(result.decoded());
    CHECK_EQ(result.speed->kilometres_per_hour(), 16);
}

TEST_CASE("displayed speed wins when physical speed differs by three kmh") {
    const std::array<std::uint8_t, 8> lower{
        0, 0x00, 0x39, 30, 0, 0, 0, 0}; // raw 912: 32.96 km/h
    const std::array<std::uint8_t, 8> upper{
        0, 0x00, 0x39, 30, 1, 0, 0, 0};

    CHECK_EQ(
        decode_vehicle_speed(speed_frame(lower))
            .speed->kilometres_per_hour(),
        30);
    CHECK_EQ(
        decode_vehicle_speed(speed_frame(upper))
            .speed->kilometres_per_hour(),
        30);
}

TEST_CASE("zero displayed speed is valid regardless of unit flag") {
    const std::array<std::uint8_t, 8> metric{
        0, 0x40, 0x1F, 0, 1, 0, 0, 0};
    const std::array<std::uint8_t, 8> imperial{
        0, 0x40, 0x1F, 0, 0, 0, 0, 0};

    CHECK_EQ(
        decode_vehicle_speed(speed_frame(metric))
            .speed->kilometres_per_hour(),
        0);
    CHECK_EQ(
        decode_vehicle_speed(speed_frame(imperial))
            .speed->kilometres_per_hour(),
        0);
}

TEST_CASE("reports Tesla's SNA displayed speed without a physical fallback") {
    const std::array<std::uint8_t, 8> data{
        0, 0xC0, 0x88, 255, 1, 0, 0, 0};

    const auto result = decode_vehicle_speed(speed_frame(data));

    CHECK_EQ(result.status, SpeedDecodeStatus::Unavailable);
    CHECK(!result.speed.has_value());
}

TEST_CASE("ignores the speed address on another DashKit bus") {
    const std::array<std::uint8_t, 8> data{
        0, 0, 0, 42, 1, 0, 0, 0};
    const CanFrameView frame{123, 1, 599, data};

    const auto result = decode_vehicle_speed(frame);

    CHECK_EQ(result.status, SpeedDecodeStatus::Ignored);
    CHECK(!result.speed.has_value());
}

TEST_CASE("ignores unrelated CAN addresses") {
    const std::array<std::uint8_t, 8> data{
        0, 0, 0, 42, 1, 0, 0, 0};
    const CanFrameView frame{123, 0, 598, data};

    const auto result = decode_vehicle_speed(frame);

    CHECK_EQ(result.status, SpeedDecodeStatus::Ignored);
}

TEST_CASE("rejects a truncated DI speed frame") {
    const std::array<std::uint8_t, 7> data{0, 0, 0, 42, 1, 0, 0};
    const CanFrameView frame{123, 0, 599, data};

    const auto result = decode_vehicle_speed(frame);

    CHECK_EQ(result.status, SpeedDecodeStatus::InvalidFrame);
    CHECK(!result.speed.has_value());
}

TEST_CASE("unavailable physical speed does not hide valid displayed speed") {
    const std::array<std::uint8_t, 8> data{
        0, 0xF0, 0xFF, 135, 0, 0, 0, 0};

    const auto result = decode_vehicle_speed(speed_frame(data));

    CHECK(result.decoded());
    CHECK_EQ(result.speed->kilometres_per_hour(), 135);
}

TEST_CASE("all displayed speed encodings ignore unit flag and unrelated bits") {
    for (std::uint16_t raw = 0; raw <= 255U; ++raw) {
        for (const std::uint8_t flags : {0U, 1U, 0xFEU, 0xFFU}) {
            const std::array<std::uint8_t, 8> data{
                0xA5, 0xFA, 0xFF, static_cast<std::uint8_t>(raw),
                flags, 0xAA, 0x55, 0xFF};
            const auto result = decode_vehicle_speed(speed_frame(data));
            if (raw == 255U) {
                CHECK_EQ(result.status, SpeedDecodeStatus::Unavailable);
            } else {
                CHECK(result.decoded());
                CHECK_EQ(result.speed->kilometres_per_hour(), raw);
            }
        }
    }
}

static_assert(noexcept(decode_vehicle_speed(CanFrameView{})));

}  // namespace
