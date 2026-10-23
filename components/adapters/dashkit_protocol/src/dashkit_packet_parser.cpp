#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"

namespace tesla_headup::dashkit::protocol {
namespace {

constexpr std::size_t timestamp_size = 4;
constexpr std::size_t address_size = 4;

std::uint32_t read_little_endian_u32(
    std::span<const std::uint8_t> bytes,
    std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

ParseResult failure(
    PacketError error,
    std::uint8_t declared_frame_count,
    std::uint8_t parsed_frame_count,
    std::size_t bytes_consumed) noexcept {
    return {
        error,
        declared_frame_count,
        parsed_frame_count,
        bytes_consumed,
    };
}

ParseResult walk_packet(
    std::span<const std::uint8_t> packet,
    FrameVisitor visitor,
    void* context) noexcept {
    if (packet.empty()) {
        return failure(PacketError::EmptyPacket, 0, 0, 0);
    }

    const auto declared_frame_count = packet.front();
    if (declared_frame_count > maximum_frame_count) {
        return failure(PacketError::TooManyFrames, declared_frame_count, 0, 1);
    }

    std::size_t offset = 1;
    for (std::uint8_t frame_index = 0;
         frame_index < declared_frame_count;
         ++frame_index) {
        if (packet.size() - offset < timestamp_size) {
            return failure(
                PacketError::TruncatedTimestamp,
                declared_frame_count,
                frame_index,
                offset);
        }
        const auto timestamp = read_little_endian_u32(packet, offset);
        offset += timestamp_size;

        if (offset == packet.size()) {
            return failure(
                PacketError::MissingBus,
                declared_frame_count,
                frame_index,
                offset);
        }
        const auto bus = packet[offset++];

        if (packet.size() - offset < address_size) {
            return failure(
                PacketError::TruncatedAddress,
                declared_frame_count,
                frame_index,
                offset);
        }
        const auto address = read_little_endian_u32(packet, offset);
        offset += address_size;

        if (offset == packet.size()) {
            return failure(
                PacketError::MissingPayloadLength,
                declared_frame_count,
                frame_index,
                offset);
        }
        const auto payload_size = packet[offset++];
        if (payload_size > maximum_can_payload_size) {
            return failure(
                PacketError::PayloadTooLarge,
                declared_frame_count,
                frame_index,
                offset);
        }
        if (packet.size() - offset < payload_size) {
            return failure(
                PacketError::TruncatedPayload,
                declared_frame_count,
                frame_index,
                offset);
        }

        if (visitor != nullptr) {
            visitor(
                context,
                CanFrameView{
                    timestamp,
                    bus,
                    address,
                    packet.subspan(offset, payload_size),
                });
        }
        offset += payload_size;
    }

    if (offset != packet.size()) {
        return failure(
            PacketError::TrailingBytes,
            declared_frame_count,
            declared_frame_count,
            offset);
    }

    return {
        PacketError::None,
        declared_frame_count,
        declared_frame_count,
        offset,
    };
}

}  // namespace

ParseResult parse_can_packet(
    std::span<const std::uint8_t> packet,
    FrameVisitor visitor,
    void* context) noexcept {
    const auto validation = walk_packet(packet, nullptr, nullptr);
    if (!validation.ok() || visitor == nullptr) {
        return validation;
    }
    return walk_packet(packet, visitor, context);
}

}  // namespace tesla_headup::dashkit::protocol
