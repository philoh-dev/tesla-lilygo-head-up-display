#include "tesla_headup/application/diagnostics/bounded_can_frame_capture.hpp"

#include <algorithm>

namespace tesla_headup::application {

CanFrameCaptureResult BoundedCanFrameCapture::capture(
    std::uint32_t source_timestamp_microseconds,
    std::uint8_t bus,
    std::uint32_t address,
    std::span<const std::uint8_t> payload,
    std::chrono::milliseconds received_at) noexcept {
    if (payload.size() > diagnostic_can_payload_capacity) {
        return CanFrameCaptureResult::InvalidPayload;
    }

    const bool clock_moved_backwards =
        window_started_ && received_at < window_started_at_;
    const bool window_elapsed =
        window_started_ &&
        received_at - window_started_at_ >= policy_.window_duration;
    if (!window_started_ || clock_moved_backwards || window_elapsed) {
        start_new_window(received_at);
    }

    if (captured_in_window_ >= policy_.maximum_frames_per_window) {
        ++suppressed_in_window_;
        return CanFrameCaptureResult::RateLimited;
    }

    CanFrameDiagnosticRecord record{
        .source_timestamp_microseconds = source_timestamp_microseconds,
        .received_at_milliseconds = received_at.count(),
        .address = address,
        .bus = bus,
        .payload_length = static_cast<std::uint8_t>(payload.size()),
    };
    std::copy(payload.begin(), payload.end(), record.payload.begin());
    sink_.write(record);
    ++captured_in_window_;
    return CanFrameCaptureResult::Captured;
}

void BoundedCanFrameCapture::start_new_window(
    std::chrono::milliseconds received_at) noexcept {
    if (suppressed_in_window_ != 0) {
        sink_.report_suppressed(suppressed_in_window_);
    }
    window_started_at_ = received_at;
    suppressed_in_window_ = 0;
    captured_in_window_ = 0;
    window_started_ = true;
}

}  // namespace tesla_headup::application
