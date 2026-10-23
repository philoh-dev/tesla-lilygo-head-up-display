#include "tesla_headup/adapters/esp_diagnostics/esp_log_can_frame_sink.hpp"

#include "esp_log.h"

#include <array>
#include <cstddef>
#include <cstdio>

namespace tesla_headup::platform {
namespace {

constexpr char tag[] = "tesla_headup_can_capture";

}  // namespace

void EspLogCanFrameSink::write(
    const application::CanFrameDiagnosticRecord& record) noexcept {
    std::array<char, application::diagnostic_can_payload_capacity * 3> text{};
    std::size_t offset = 0;
    for (std::uint8_t index = 0; index < record.payload_length; ++index) {
        const int written = std::snprintf(
            text.data() + offset,
            text.size() - offset,
            index == 0 ? "%02X" : " %02X",
            static_cast<unsigned>(record.payload[index]));
        if (written <= 0) {
            break;
        }
        offset += static_cast<std::size_t>(written);
    }

    ESP_LOGI(
        tag,
        "CAN local_ms=%lld source_us=%lu bus=%u id=0x%08lX len=%u data=[%s]",
        static_cast<long long>(record.received_at_milliseconds),
        static_cast<unsigned long>(record.source_timestamp_microseconds),
        static_cast<unsigned>(record.bus),
        static_cast<unsigned long>(record.address),
        static_cast<unsigned>(record.payload_length),
        text.data());
}

void EspLogCanFrameSink::report_suppressed(
    std::uint32_t frame_count) noexcept {
    ESP_LOGW(
        tag,
        "Suppressed %lu CAN frames in the previous capture window",
        static_cast<unsigned long>(frame_count));
}

}  // namespace tesla_headup::platform
