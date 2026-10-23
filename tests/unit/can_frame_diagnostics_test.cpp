#include "test.hpp"

#include "synthetic_dashkit_packets.hpp"
#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/diagnostics/bounded_can_frame_capture.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <span>

namespace {

using namespace std::chrono_literals;

class RecordingDiagnosticSink final
    : public tesla_headup::application::CanFrameDiagnosticSink {
public:
    void write(
        const tesla_headup::application::CanFrameDiagnosticRecord& record)
        noexcept override {
        if (record_count < records.size()) {
            records[record_count++] = record;
        }
    }

    void report_suppressed(std::uint32_t frame_count) noexcept override {
        reported_suppressed += frame_count;
        ++suppression_reports;
    }

    std::array<tesla_headup::application::CanFrameDiagnosticRecord, 16> records{};
    std::size_t record_count{0};
    std::uint32_t reported_suppressed{0};
    std::uint32_t suppression_reports{0};
};

TEST_CASE("CAN diagnostics copy every bounded frame field") {
    RecordingDiagnosticSink sink;
    tesla_headup::application::BoundedCanFrameCapture capture{sink};
    constexpr std::array<std::uint8_t, 3> payload{0x01, 0xA5, 0xFF};

    const auto result = capture.capture(
        123'456, 1, 0x3E9, payload, 42ms);

    CHECK_EQ(result, tesla_headup::application::CanFrameCaptureResult::Captured);
    CHECK_EQ(sink.record_count, 1U);
    const auto& record = sink.records[0];
    CHECK_EQ(record.source_timestamp_microseconds, 123'456U);
    CHECK_EQ(record.received_at_milliseconds, 42);
    CHECK_EQ(record.bus, 1);
    CHECK_EQ(record.address, 0x3E9U);
    CHECK_EQ(record.payload_length, 3);
    CHECK_EQ(record.payload[0], 0x01);
    CHECK_EQ(record.payload[1], 0xA5);
    CHECK_EQ(record.payload[2], 0xFF);
}

TEST_CASE("CAN diagnostics rate limit and summarize each window") {
    RecordingDiagnosticSink sink;
    tesla_headup::application::BoundedCanFrameCapture capture{
        sink,
        {
            .maximum_frames_per_window = 2,
            .window_duration = 1s,
        }};
    constexpr std::array<std::uint8_t, 1> payload{0x01};

    CHECK_EQ(
        capture.capture(1, 0, 100, payload, 0ms),
        tesla_headup::application::CanFrameCaptureResult::Captured);
    CHECK_EQ(
        capture.capture(2, 0, 101, payload, 1ms),
        tesla_headup::application::CanFrameCaptureResult::Captured);
    CHECK_EQ(
        capture.capture(3, 0, 102, payload, 2ms),
        tesla_headup::application::CanFrameCaptureResult::RateLimited);
    CHECK_EQ(
        capture.capture(4, 0, 103, payload, 3ms),
        tesla_headup::application::CanFrameCaptureResult::RateLimited);
    CHECK_EQ(sink.record_count, 2U);
    CHECK_EQ(sink.suppression_reports, 0U);

    CHECK_EQ(
        capture.capture(5, 1, 104, payload, 1s),
        tesla_headup::application::CanFrameCaptureResult::Captured);
    CHECK_EQ(sink.record_count, 3U);
    CHECK_EQ(sink.suppression_reports, 1U);
    CHECK_EQ(sink.reported_suppressed, 2U);
}

TEST_CASE("CAN diagnostics reject payloads larger than classic CAN") {
    RecordingDiagnosticSink sink;
    tesla_headup::application::BoundedCanFrameCapture capture{sink};
    constexpr std::array<std::uint8_t, 9> payload{};

    CHECK_EQ(
        capture.capture(0, 0, 0, payload, 0ms),
        tesla_headup::application::CanFrameCaptureResult::InvalidPayload);
    CHECK_EQ(sink.record_count, 0U);
}

TEST_CASE("telemetry ingestion captures only atomically valid packets") {
    RecordingDiagnosticSink sink;
    tesla_headup::application::BoundedCanFrameCapture capture{sink};
    tesla_headup::application::TelemetryStore telemetry;
    tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry, &capture};

    const auto malformed =
        tesla_headup::test::fixtures::truncated_active_drive();
    CHECK(!ingestor.ingest(malformed, 0ms).accepted_packet());
    CHECK_EQ(sink.record_count, 0U);

    CHECK(ingestor.ingest(
              tesla_headup::test::fixtures::active_drive, 1ms)
              .accepted_packet());
    CHECK_EQ(sink.record_count, 7U);
}

}  // namespace
