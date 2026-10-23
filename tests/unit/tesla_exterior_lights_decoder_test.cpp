#include "test.hpp"

#include "tesla_headup/adapters/tesla_decoder/tesla_exterior_lights_decoder.hpp"

#include <array>
#include <cstdint>

namespace {

using tesla_headup::dashkit::protocol::CanFrameView;
using tesla_headup::tesla::ExteriorLampValidity;
using tesla_headup::tesla::ExteriorLightDecodeStatus;
using tesla_headup::tesla::decode_exterior_lights;

TEST_CASE("decodes physical front exterior lamp feedback") {
    const std::array<std::uint8_t, 8> data{
        0, 0, 0, 0x10, 0x04, 0x01, 0x40, 0};

    const auto result = decode_exterior_lights(
        CanFrameView{123, 1, 1009, data});

    CHECK_EQ(result.status, ExteriorLightDecodeStatus::FrontLighting);
    CHECK(result.position_lights.active);
    CHECK(result.low_beam.active);
    CHECK(result.high_beam.active);
    CHECK(result.front_fog.active);
    CHECK_EQ(result.position_lights.validity, ExteriorLampValidity::Valid);
    CHECK_EQ(result.source_timestamp_microseconds, 123U);
}

TEST_CASE("decodes Juniper front lighting from forwarded DAS body controls") {
    const std::array<std::uint8_t, 8> active{
        0x01, 0x10, 0, 0, 0, 0, 0, 0};
    const auto result = decode_exterior_lights(
        CanFrameView{123, 1, 1001, active});

    CHECK_EQ(result.status, ExteriorLightDecodeStatus::FrontLightingRequests);
    CHECK(result.position_lights.active);
    CHECK(result.low_beam.active);
    CHECK(result.high_beam.active);
    CHECK_EQ(result.front_fog.validity, ExteriorLampValidity::Unavailable);

    const std::array<std::uint8_t, 8> tail_lights_only{
        0x02, 0x08, 0, 0, 0, 0, 0, 0};
    const auto tail = decode_exterior_lights(
        CanFrameView{124, 1, 1001, tail_lights_only});
    CHECK(tail.position_lights.active);
    CHECK(!tail.low_beam.active);
    CHECK(!tail.high_beam.active);
}

TEST_CASE("Juniper unavailable front-light states fail dark") {
    const std::array<std::uint8_t, 8> unavailable{
        0x03, 0x18, 0, 0, 0, 0, 0, 0};
    const auto result = decode_exterior_lights(
        CanFrameView{1, 1, 1001, unavailable});

    CHECK_EQ(result.position_lights.validity,
             ExteriorLampValidity::Unavailable);
    CHECK_EQ(result.low_beam.validity, ExteriorLampValidity::Unavailable);
    CHECK_EQ(result.high_beam.validity, ExteriorLampValidity::Unavailable);
}

TEST_CASE("either side can activate a paired front lamp") {
    const std::array<std::uint8_t, 8> data{
        0, 0, 0, 0x40, 0x01, 0x04, 0, 0x01};

    const auto result = decode_exterior_lights(
        CanFrameView{1, 1, 1009, data});

    CHECK(result.position_lights.active);
    CHECK(result.low_beam.active);
    CHECK(result.high_beam.active);
    CHECK(result.front_fog.active);
}

TEST_CASE("front lamp fault and unavailable values remain explicit") {
    const std::array<std::uint8_t, 8> data{
        0, 0, 0, 0x20, 0x03, 0, 0, 0};

    const auto result = decode_exterior_lights(
        CanFrameView{1, 1, 1009, data});

    CHECK_EQ(result.low_beam.validity, ExteriorLampValidity::Fault);
    CHECK_EQ(result.high_beam.validity, ExteriorLampValidity::Unavailable);
    CHECK(!result.low_beam.active);
    CHECK(!result.high_beam.active);
}

TEST_CASE("decodes rear fog feedback from VCRIGHT light status") {
    const std::array<std::uint8_t, 2> on{0, 0x01};
    const std::array<std::uint8_t, 8> padded_on{0, 0x01, 0, 0, 0, 0, 0, 0};
    const std::array<std::uint8_t, 4> juniper_on{0, 0x01, 0, 0};
    const std::array<std::uint8_t, 2> fault{0, 0x02};
    const std::array<std::uint8_t, 2> unavailable{0, 0x03};

    const auto active = decode_exterior_lights(
        CanFrameView{1, 1, 995, on});
    CHECK_EQ(active.status, ExteriorLightDecodeStatus::RearLighting);
    CHECK(active.rear_fog.active);
    CHECK_EQ(active.rear_fog.validity, ExteriorLampValidity::Valid);
    const auto padded = decode_exterior_lights(
        CanFrameView{1, 1, 995, padded_on});
    CHECK_EQ(padded.status, ExteriorLightDecodeStatus::RearLighting);
    CHECK(padded.rear_fog.active);
    CHECK(decode_exterior_lights(CanFrameView{1, 1, 995, juniper_on})
              .rear_fog.active);
    CHECK_EQ(
        decode_exterior_lights(CanFrameView{1, 1, 995, fault})
            .rear_fog.validity,
        ExteriorLampValidity::Fault);
    CHECK_EQ(
        decode_exterior_lights(CanFrameView{1, 1, 995, unavailable})
            .rear_fog.validity,
        ExteriorLampValidity::Unavailable);
}

TEST_CASE("decodes provisional adaptive high-beam setting") {
    const std::array<std::uint8_t, 8> enabled{
        0x08, 0, 0, 0, 0, 0x02, 0, 0};
    const std::array<std::uint8_t, 8> disabled{};

    const auto active = decode_exterior_lights(
        CanFrameView{1, 1, 627, enabled});
    const auto inactive = decode_exterior_lights(
        CanFrameView{2, 1, 627, disabled});

    CHECK_EQ(active.status, ExteriorLightDecodeStatus::LightingControls);
    CHECK(active.front_fog.active);
    CHECK(active.adaptive_high_beam_enabled);
    CHECK(!inactive.adaptive_high_beam_enabled);
}

TEST_CASE("exterior-light decoder ignores unrelated CAN traffic") {
    const std::array<std::uint8_t, 8> data{};

    CHECK_EQ(
        decode_exterior_lights(CanFrameView{1, 0, 1009, data}).status,
        ExteriorLightDecodeStatus::Ignored);
    CHECK_EQ(
        decode_exterior_lights(CanFrameView{1, 1, 1008, data}).status,
        ExteriorLightDecodeStatus::Ignored);
}

TEST_CASE("exterior-light decoder rejects truncation and accepts padding") {
    const std::array<std::uint8_t, 7> short_data{};
    const std::array<std::uint8_t, 1> short_rear_data{};

    CHECK_EQ(
        decode_exterior_lights(CanFrameView{1, 1, 1001, short_data}).status,
        ExteriorLightDecodeStatus::InvalidFrame);
    CHECK_EQ(
        decode_exterior_lights(CanFrameView{1, 1, 1009, short_data}).status,
        ExteriorLightDecodeStatus::InvalidFrame);
    CHECK_EQ(
        decode_exterior_lights(CanFrameView{1, 1, 995, short_rear_data}).status,
        ExteriorLightDecodeStatus::InvalidFrame);
    CHECK_EQ(
        decode_exterior_lights(CanFrameView{1, 1, 627, short_data}).status,
        ExteriorLightDecodeStatus::InvalidFrame);
}

static_assert(noexcept(decode_exterior_lights(CanFrameView{})));

}  // namespace
