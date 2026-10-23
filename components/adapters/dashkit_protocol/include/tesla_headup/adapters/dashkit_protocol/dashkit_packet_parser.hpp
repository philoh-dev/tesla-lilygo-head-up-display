#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tesla_headup::dashkit::protocol {

inline constexpr std::uint8_t maximum_frame_count = 16;
inline constexpr std::uint8_t maximum_can_payload_size = 8;

struct CanFrameView {
    std::uint32_t timestamp_microseconds{};
    std::uint8_t bus{};
    std::uint32_t address{};
    std::span<const std::uint8_t> data{};
};

enum class PacketError : std::uint8_t {
    None,
    EmptyPacket,
    TooManyFrames,
    TruncatedTimestamp,
    MissingBus,
    TruncatedAddress,
    MissingPayloadLength,
    PayloadTooLarge,
    TruncatedPayload,
    TrailingBytes,
};

struct ParseResult {
    PacketError error{PacketError::None};
    std::uint8_t declared_frame_count{};
    std::uint8_t parsed_frame_count{};
    std::size_t bytes_consumed{};

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error == PacketError::None;
    }
};

using FrameVisitor = void (*)(void* context, const CanFrameView& frame) noexcept;

// Validates the complete packet before visiting any frames. Frame data is a
// non-owning view into packet and remains valid only as long as packet does.
// Passing a null visitor performs validation without emitting frames.
[[nodiscard]] ParseResult parse_can_packet(
    std::span<const std::uint8_t> packet,
    FrameVisitor visitor = nullptr,
    void* context = nullptr) noexcept;

}  // namespace tesla_headup::dashkit::protocol
