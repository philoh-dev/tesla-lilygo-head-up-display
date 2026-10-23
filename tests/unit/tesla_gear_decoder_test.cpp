#include "test.hpp"

#include "tesla_headup/adapters/tesla_decoder/tesla_gear_decoder.hpp"

#include <array>
#include <span>

namespace {

using tesla_headup::dashkit::protocol::CanFrameView;
using tesla_headup::domain::SelectedGear;
using tesla_headup::tesla::GearDecodeStatus;
using tesla_headup::tesla::decode_selected_gear;

CanFrameView gear_frame(
    const std::array<std::uint8_t, 8>& data,
    std::uint32_t timestamp = 123) {
    return {timestamp, 0, 0x118, data};
}

TEST_CASE("decodes every defined Tesla DI gear value") {
    constexpr std::array expected{
        SelectedGear::Park,
        SelectedGear::Reverse,
        SelectedGear::Neutral,
        SelectedGear::Drive,
    };
    for (std::uint8_t raw = 1; raw <= 4; ++raw) {
        std::array<std::uint8_t, 8> data{};
        data[2] = static_cast<std::uint8_t>(raw << 5U);
        const auto result = decode_selected_gear(gear_frame(data, raw));
        CHECK(result.decoded());
        CHECK_EQ(*result.gear, expected[raw - 1]);
        CHECK_EQ(result.source_timestamp_microseconds, raw);
    }
}

TEST_CASE("reports Tesla invalid and SNA gears as unavailable") {
    for (const std::uint8_t raw : {0U, 7U}) {
        std::array<std::uint8_t, 8> data{};
        data[2] = static_cast<std::uint8_t>(raw << 5U);
        const auto result = decode_selected_gear(gear_frame(data));
        CHECK_EQ(result.status, GearDecodeStatus::Unavailable);
        CHECK(!result.gear.has_value());
    }
}

TEST_CASE("rejects undefined Tesla gear encodings") {
    for (const std::uint8_t raw : {5U, 6U}) {
        std::array<std::uint8_t, 8> data{};
        data[2] = static_cast<std::uint8_t>(raw << 5U);
        CHECK_EQ(
            decode_selected_gear(gear_frame(data)).status,
            GearDecodeStatus::InvalidValue);
    }
}

TEST_CASE("gear decoder validates bus address and payload length") {
    const std::array<std::uint8_t, 8> data{0, 0, 0x80, 0, 0, 0, 0, 0};
    CHECK_EQ(
        decode_selected_gear(CanFrameView{0, 1, 0x118, data}).status,
        GearDecodeStatus::Ignored);
    CHECK_EQ(
        decode_selected_gear(CanFrameView{0, 0, 0x119, data}).status,
        GearDecodeStatus::Ignored);
    CHECK_EQ(
        decode_selected_gear(
            CanFrameView{0, 0, 0x118, std::span{data}.first(7)})
            .status,
        GearDecodeStatus::InvalidFrame);
}

static_assert(noexcept(decode_selected_gear(CanFrameView{})));

}  // namespace
