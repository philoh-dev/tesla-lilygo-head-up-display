#pragma once

#include "tesla_headup/adapters/dashkit_protocol/dashkit_packet_parser.hpp"
#include "tesla_headup/application/diagnostics/bounded_can_frame_capture.hpp"
#include "tesla_headup/application/telemetry/telemetry_store.hpp"

#include <cstdint>
#include <span>

namespace tesla_headup::dashkit {

struct TelemetryIngestResult {
    std::uint8_t decoded_battery_charge_frames{};
    std::uint8_t unavailable_battery_charge_frames{};
    std::uint8_t rejected_battery_charge_frames{};
    std::uint8_t decoded_power_frames{};
    std::uint8_t unavailable_power_frames{};
    std::uint8_t rejected_power_frames{};
    protocol::PacketError packet_error{protocol::PacketError::None};
    std::uint8_t decoded_speed_frames{};
    std::uint8_t unavailable_speed_frames{};
    std::uint8_t rejected_speed_frames{};
    std::uint8_t decoded_gear_frames{};
    std::uint8_t unavailable_gear_frames{};
    std::uint8_t rejected_gear_frames{};
    std::uint8_t decoded_turn_signal_frames{};
    std::uint8_t unavailable_turn_signal_frames{};
    std::uint8_t rejected_turn_signal_frames{};
    std::uint8_t decoded_driver_assistance_frames{};
    std::uint8_t unavailable_driver_assistance_fields{};
    std::uint8_t faulted_driver_assistance_fields{};
    std::uint8_t rejected_driver_assistance_frames{};
    std::uint8_t decoded_speed_limit_frames{};
    std::uint8_t unavailable_speed_limit_frames{};
    std::uint8_t rejected_speed_limit_frames{};

    [[nodiscard]] constexpr bool accepted_packet() const noexcept {
        return packet_error == protocol::PacketError::None;
    }
};

// Bridges one atomically validated DashKit notification into the application
// telemetry store. Every CAN frame is offered to the compact Tesla decoders.
class DashKitTelemetryIngestor final {
public:
    explicit constexpr DashKitTelemetryIngestor(
        application::TelemetryStore& telemetry,
        application::BoundedCanFrameCapture* diagnostic_capture = nullptr)
        noexcept
        : telemetry_(telemetry), diagnostic_capture_(diagnostic_capture) {}

    [[nodiscard]] TelemetryIngestResult ingest(
        std::span<const std::uint8_t> payload,
        application::TelemetryTime received_at) noexcept;

private:
    application::TelemetryStore& telemetry_;
    application::BoundedCanFrameCapture* diagnostic_capture_;
};

}  // namespace tesla_headup::dashkit
