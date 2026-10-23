#pragma once

#include "tesla_headup/ports/can_frame_diagnostic_sink.hpp"

#include <chrono>
#include <cstdint>
#include <span>

namespace tesla_headup::application {

struct CanFrameCapturePolicy {
    std::uint16_t maximum_frames_per_window{32};
    std::chrono::milliseconds window_duration{1'000};
};

enum class CanFrameCaptureResult : std::uint8_t {
    Captured,
    RateLimited,
    InvalidPayload,
};

class BoundedCanFrameCapture final {
public:
    explicit constexpr BoundedCanFrameCapture(
        CanFrameDiagnosticSink& sink,
        CanFrameCapturePolicy policy = {}) noexcept
        : sink_(sink), policy_(policy) {}

    [[nodiscard]] CanFrameCaptureResult capture(
        std::uint32_t source_timestamp_microseconds,
        std::uint8_t bus,
        std::uint32_t address,
        std::span<const std::uint8_t> payload,
        std::chrono::milliseconds received_at) noexcept;

private:
    void start_new_window(std::chrono::milliseconds received_at) noexcept;

    CanFrameDiagnosticSink& sink_;
    CanFrameCapturePolicy policy_;
    std::chrono::milliseconds window_started_at_{};
    std::uint32_t suppressed_in_window_{0};
    std::uint16_t captured_in_window_{0};
    bool window_started_{false};
};

}  // namespace tesla_headup::application
