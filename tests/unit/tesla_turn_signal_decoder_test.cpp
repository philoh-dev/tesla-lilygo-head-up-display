#include "test.hpp"

#include "tesla_headup/adapters/tesla_decoder/tesla_turn_signal_decoder.hpp"

#include <array>
#include <cstdint>

namespace {

using tesla_headup::dashkit::protocol::CanFrameView;
using tesla_headup::tesla::TurnSignalDecodeStatus;
using tesla_headup::tesla::TurnSignalSide;
using tesla_headup::tesla::decode_turn_signal_lamp;

TEST_CASE("decodes Tesla left turn lamp phases") {
    const std::array<std::uint8_t, 7> on{0x10, 0, 0, 0, 0, 0, 0};
    const std::array<std::uint8_t, 7> off{0x00, 0, 0, 0, 0, 0, 0};

    const auto illuminated =
        decode_turn_signal_lamp(CanFrameView{123, 1, 994, on});
    const auto dark = decode_turn_signal_lamp(CanFrameView{124, 1, 994, off});

    CHECK(illuminated.decoded());
    CHECK_EQ(illuminated.side, TurnSignalSide::Left);
    CHECK(illuminated.lamp_illuminated);
    CHECK_EQ(illuminated.source_timestamp_microseconds, 123U);
    CHECK(dark.decoded());
    CHECK(!dark.lamp_illuminated);
}

TEST_CASE("decodes Tesla right turn lamp phases") {
    const std::array<std::uint8_t, 2> on{0x10, 0};

    const auto result =
        decode_turn_signal_lamp(CanFrameView{456, 1, 995, on});

    CHECK(result.decoded());
    CHECK_EQ(result.side, TurnSignalSide::Right);
    CHECK(result.lamp_illuminated);
}

TEST_CASE("reports fault and unavailable lamp states separately") {
    const std::array<std::uint8_t, 7> fault{0x20, 0, 0, 0, 0, 0, 0};
    const std::array<std::uint8_t, 2> unavailable{0x30, 0};

    CHECK_EQ(
        decode_turn_signal_lamp(CanFrameView{1, 1, 994, fault}).status,
        TurnSignalDecodeStatus::Fault);
    CHECK_EQ(
        decode_turn_signal_lamp(CanFrameView{2, 1, 995, unavailable}).status,
        TurnSignalDecodeStatus::Unavailable);
}

TEST_CASE("turn lamp decoder ignores unrelated bus and addresses") {
    const std::array<std::uint8_t, 7> data{0x10, 0, 0, 0, 0, 0, 0};

    CHECK_EQ(
        decode_turn_signal_lamp(CanFrameView{1, 0, 994, data}).status,
        TurnSignalDecodeStatus::Ignored);
    CHECK_EQ(
        decode_turn_signal_lamp(CanFrameView{1, 1, 993, data}).status,
        TurnSignalDecodeStatus::Ignored);
}

TEST_CASE("turn lamp decoder rejects truncation and accepts adapter padding") {
    const std::array<std::uint8_t, 2> too_short_left{0x10, 0};
    const std::array<std::uint8_t, 1> too_short_right{0x10};
    const std::array<std::uint8_t, 8> padded_left{
        0x10, 0, 0, 0, 0, 0, 0, 0};
    const std::array<std::uint8_t, 8> padded_right{
        0x10, 0, 0, 0, 0, 0, 0, 0};

    CHECK_EQ(
        decode_turn_signal_lamp(
            CanFrameView{1, 1, 994, too_short_left}).status,
        TurnSignalDecodeStatus::InvalidFrame);
    CHECK_EQ(
        decode_turn_signal_lamp(
            CanFrameView{1, 1, 995, too_short_right}).status,
        TurnSignalDecodeStatus::InvalidFrame);
    CHECK(decode_turn_signal_lamp(
              CanFrameView{1, 1, 994, padded_left})
              .decoded());
    const auto right = decode_turn_signal_lamp(
        CanFrameView{1, 1, 995, padded_right});
    CHECK(right.decoded());
    CHECK_EQ(right.side, TurnSignalSide::Right);
    CHECK(right.lamp_illuminated);
}

static_assert(noexcept(decode_turn_signal_lamp(CanFrameView{})));

}  // namespace
