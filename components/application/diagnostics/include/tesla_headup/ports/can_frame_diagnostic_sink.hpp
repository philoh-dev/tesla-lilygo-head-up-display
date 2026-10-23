#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tesla_headup::application {

inline constexpr std::size_t diagnostic_can_payload_capacity = 8;

struct CanFrameDiagnosticRecord {
    std::uint32_t source_timestamp_microseconds{};
    std::int64_t received_at_milliseconds{};
    std::uint32_t address{};
    std::uint8_t bus{};
    std::uint8_t payload_length{};
    std::array<std::uint8_t, diagnostic_can_payload_capacity> payload{};
};

class CanFrameDiagnosticSink {
public:
    virtual ~CanFrameDiagnosticSink() = default;

    virtual void write(const CanFrameDiagnosticRecord& record) noexcept = 0;
    virtual void report_suppressed(std::uint32_t frame_count) noexcept = 0;
};

}  // namespace tesla_headup::application
