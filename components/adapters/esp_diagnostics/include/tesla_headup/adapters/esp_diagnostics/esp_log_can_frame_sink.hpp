#pragma once

#include "tesla_headup/ports/can_frame_diagnostic_sink.hpp"

namespace tesla_headup::platform {

class EspLogCanFrameSink final
    : public application::CanFrameDiagnosticSink {
public:
    void write(
        const application::CanFrameDiagnosticRecord& record) noexcept override;
    void report_suppressed(std::uint32_t frame_count) noexcept override;
};

}  // namespace tesla_headup::platform
