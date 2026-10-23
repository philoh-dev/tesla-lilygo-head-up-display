#include "test.hpp"

#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using tesla_headup::dashkit::protocol::CanFrameView;
using tesla_headup::dashkit::protocol::PacketError;
using tesla_headup::dashkit::protocol::ParseResult;
using tesla_headup::dashkit::protocol::maximum_can_payload_size;
using tesla_headup::dashkit::protocol::maximum_frame_count;
using tesla_headup::dashkit::protocol::parse_can_packet;

struct RecordedFrame {
    std::uint32_t timestamp_microseconds{};
    std::uint8_t bus{};
    std::uint32_t address{};
    std::vector<std::uint8_t> data;
    const std::uint8_t* source_data{};
};

struct RecordingVisitor {
    std::vector<RecordedFrame> frames;

    static void visit(void* context, const CanFrameView& frame) noexcept {
        auto& self = *static_cast<RecordingVisitor*>(context);
        self.frames.push_back({
            frame.timestamp_microseconds,
            frame.bus,
            frame.address,
            {frame.data.begin(), frame.data.end()},
            frame.data.data(),
        });
    }
};

ParseResult parse(
    std::span<const std::uint8_t> packet,
    RecordingVisitor& visitor) {
    return parse_can_packet(packet, RecordingVisitor::visit, &visitor);
}

std::vector<std::uint8_t> valid_two_frame_packet() {
    return {
        2,
        0x78, 0x56, 0x34, 0x12,
        0,
        0x57, 0x02, 0x00, 0x00,
        3,
        0xAA, 0xBB, 0xCC,
        0xEF, 0xCD, 0xAB, 0x90,
        1,
        0xE3, 0x03, 0x00, 0x00,
        2,
        0x10, 0x20,
    };
}

TEST_CASE("an empty DashKit batch is valid and emits no frames") {
    const std::array<std::uint8_t, 1> packet{0};
    RecordingVisitor visitor;

    const auto result = parse(packet, visitor);

    CHECK(result.ok());
    CHECK_EQ(result.declared_frame_count, 0);
    CHECK_EQ(result.parsed_frame_count, 0);
    CHECK_EQ(result.bytes_consumed, packet.size());
    CHECK(visitor.frames.empty());
}

TEST_CASE("decodes all fields and little-endian integers from multiple frames") {
    const auto packet = valid_two_frame_packet();
    RecordingVisitor visitor;

    const auto result = parse(packet, visitor);

    CHECK(result.ok());
    CHECK_EQ(result.declared_frame_count, 2);
    CHECK_EQ(result.parsed_frame_count, 2);
    CHECK_EQ(result.bytes_consumed, packet.size());
    CHECK_EQ(visitor.frames.size(), 2U);
    CHECK_EQ(visitor.frames[0].timestamp_microseconds, 0x12345678U);
    CHECK_EQ(visitor.frames[0].bus, 0);
    CHECK_EQ(visitor.frames[0].address, 599U);
    CHECK_EQ(visitor.frames[0].data.size(), 3U);
    CHECK_EQ(visitor.frames[0].data[0], 0xAA);
    CHECK_EQ(visitor.frames[0].data[2], 0xCC);
    CHECK_EQ(visitor.frames[1].timestamp_microseconds, 0x90ABCDEFU);
    CHECK_EQ(visitor.frames[1].bus, 1);
    CHECK_EQ(visitor.frames[1].address, 995U);
    CHECK_EQ(visitor.frames[1].data[1], 0x20);
}

TEST_CASE("frame payloads are non-owning views into the notification") {
    const auto packet = valid_two_frame_packet();
    RecordingVisitor visitor;

    CHECK(parse(packet, visitor).ok());

    CHECK(visitor.frames[0].source_data == packet.data() + 11);
    CHECK(visitor.frames[1].source_data == packet.data() + 24);
}

TEST_CASE("accepts zero-length CAN data") {
    const std::array<std::uint8_t, 11> packet{
        1,
        0, 0, 0, 0,
        1,
        0xFF, 0xFF, 0xFF, 0xFF,
        0,
    };
    RecordingVisitor visitor;

    const auto result = parse(packet, visitor);

    CHECK(result.ok());
    CHECK_EQ(visitor.frames.size(), 1U);
    CHECK(visitor.frames.front().data.empty());
    CHECK_EQ(visitor.frames.front().address, 0xFFFFFFFFU);
}

TEST_CASE("accepts the protocol maximum frame count and payload size") {
    std::vector<std::uint8_t> packet;
    packet.reserve(1 + maximum_frame_count * (10 + maximum_can_payload_size));
    packet.push_back(maximum_frame_count);
    for (std::uint8_t frame = 0; frame < maximum_frame_count; ++frame) {
        packet.insert(packet.end(), {frame, 0, 0, 0, 1, frame, 0, 0, 0});
        packet.push_back(maximum_can_payload_size);
        for (std::uint8_t byte = 0; byte < maximum_can_payload_size; ++byte) {
            packet.push_back(static_cast<std::uint8_t>(frame + byte));
        }
    }
    RecordingVisitor visitor;

    const auto result = parse(packet, visitor);

    CHECK(result.ok());
    CHECK_EQ(visitor.frames.size(), maximum_frame_count);
    CHECK_EQ(visitor.frames.back().data.size(), maximum_can_payload_size);
}

TEST_CASE("a null visitor validates without emitting frames") {
    const auto packet = valid_two_frame_packet();

    const auto result = parse_can_packet(packet);

    CHECK(result.ok());
    CHECK_EQ(result.parsed_frame_count, 2);
}

TEST_CASE("rejects a notification with no frame-count byte") {
    const auto result = parse_can_packet({});

    CHECK_EQ(result.error, PacketError::EmptyPacket);
    CHECK_EQ(result.bytes_consumed, 0U);
}

TEST_CASE("rejects frame counts above the firmware protocol limit") {
    const std::array<std::uint8_t, 1> packet{
        static_cast<std::uint8_t>(maximum_frame_count + 1)};

    const auto result = parse_can_packet(packet);

    CHECK_EQ(result.error, PacketError::TooManyFrames);
    CHECK_EQ(result.declared_frame_count, maximum_frame_count + 1);
    CHECK_EQ(result.parsed_frame_count, 0);
    CHECK_EQ(result.bytes_consumed, 1U);
}

TEST_CASE("reports each truncated fixed-width field precisely") {
    const std::array<std::uint8_t, 11> packet{
        1,
        1, 2, 3, 4,
        5,
        6, 7, 8, 9,
        0,
    };

    for (std::size_t size = 1; size < 5; ++size) {
        CHECK_EQ(
            parse_can_packet(std::span{packet}.first(size)).error,
            PacketError::TruncatedTimestamp);
    }
    CHECK_EQ(
        parse_can_packet(std::span{packet}.first(5)).error,
        PacketError::MissingBus);
    for (std::size_t size = 6; size < 10; ++size) {
        CHECK_EQ(
            parse_can_packet(std::span{packet}.first(size)).error,
            PacketError::TruncatedAddress);
    }
    CHECK_EQ(
        parse_can_packet(std::span{packet}.first(10)).error,
        PacketError::MissingPayloadLength);
}

TEST_CASE("rejects CAN payload lengths above eight bytes") {
    const std::array<std::uint8_t, 11> packet{
        1,
        0, 0, 0, 0,
        0,
        0, 0, 0, 0,
        static_cast<std::uint8_t>(maximum_can_payload_size + 1),
    };

    const auto result = parse_can_packet(packet);

    CHECK_EQ(result.error, PacketError::PayloadTooLarge);
    CHECK_EQ(result.bytes_consumed, packet.size());
}

TEST_CASE("rejects every truncated CAN payload prefix") {
    const std::array<std::uint8_t, 19> packet{
        1,
        0, 0, 0, 0,
        0,
        0, 0, 0, 0,
        8,
        0, 1, 2, 3, 4, 5, 6, 7,
    };

    for (std::size_t size = 11; size < packet.size(); ++size) {
        const auto result = parse_can_packet(std::span{packet}.first(size));
        CHECK_EQ(result.error, PacketError::TruncatedPayload);
        CHECK_EQ(result.bytes_consumed, 11U);
    }
}

TEST_CASE("rejects undeclared trailing bytes") {
    const std::array<std::uint8_t, 2> packet{0, 0xAA};

    const auto result = parse_can_packet(packet);

    CHECK_EQ(result.error, PacketError::TrailingBytes);
    CHECK_EQ(result.parsed_frame_count, 0);
    CHECK_EQ(result.bytes_consumed, 1U);
}

TEST_CASE("malformed later frames never cause partial visitor side effects") {
    auto packet = valid_two_frame_packet();
    packet.pop_back();
    RecordingVisitor visitor;

    const auto result = parse(packet, visitor);

    CHECK_EQ(result.error, PacketError::TruncatedPayload);
    CHECK_EQ(result.parsed_frame_count, 1);
    CHECK(visitor.frames.empty());
}

TEST_CASE("every proper prefix of a multi-frame packet is rejected atomically") {
    const auto packet = valid_two_frame_packet();
    for (std::size_t size = 0; size < packet.size(); ++size) {
        RecordingVisitor visitor;

        const auto result = parse(std::span{packet}.first(size), visitor);

        CHECK(!result.ok());
        CHECK(visitor.frames.empty());
    }
}

static_assert(noexcept(parse_can_packet({})));

}  // namespace
