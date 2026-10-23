#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"

#include "tesla_headup/adapters/tesla_decoder/tesla_driver_assistance_decoder.hpp"
#include "tesla_headup/adapters/tesla_decoder/tesla_battery_charge_decoder.hpp"
#include "tesla_headup/adapters/tesla_decoder/tesla_exterior_lights_decoder.hpp"
#include "tesla_headup/adapters/tesla_decoder/tesla_gear_decoder.hpp"
#include "tesla_headup/adapters/tesla_decoder/tesla_power_decoder.hpp"
#include "tesla_headup/adapters/tesla_decoder/tesla_speed_decoder.hpp"
#include "tesla_headup/adapters/tesla_decoder/tesla_turn_signal_decoder.hpp"

namespace tesla_headup::dashkit {
namespace {

struct IngestContext {
    application::TelemetryStore& telemetry;
    application::BoundedCanFrameCapture* diagnostic_capture;
    application::TelemetryTime received_at;
    TelemetryIngestResult& result;
};

application::TurnSignalLampSide application_side(
    tesla::TurnSignalSide side) noexcept {
    return side == tesla::TurnSignalSide::Left
               ? application::TurnSignalLampSide::Left
               : application::TurnSignalLampSide::Right;
}

void ingest_speed(
    IngestContext& context,
    const protocol::CanFrameView& frame) noexcept {
    const auto decoded = tesla::decode_vehicle_speed(frame);
    switch (decoded.status) {
    case tesla::SpeedDecodeStatus::Ignored:
        return;
    case tesla::SpeedDecodeStatus::Decoded:
        if (context.telemetry.update_vehicle_speed(
                *decoded.speed,
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.decoded_speed_frames;
        }
        return;
    case tesla::SpeedDecodeStatus::Unavailable:
        if (context.telemetry.mark_vehicle_speed_unavailable(
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.unavailable_speed_frames;
        }
        return;
    case tesla::SpeedDecodeStatus::InvalidFrame:
    case tesla::SpeedDecodeStatus::OutOfRange:
        ++context.result.rejected_speed_frames;
        return;
    }
}

void ingest_gear(
    IngestContext& context,
    const protocol::CanFrameView& frame) noexcept {
    const auto decoded = tesla::decode_selected_gear(frame);
    switch (decoded.status) {
    case tesla::GearDecodeStatus::Ignored:
        return;
    case tesla::GearDecodeStatus::Decoded:
        if (context.telemetry.update_selected_gear(
                *decoded.gear,
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.decoded_gear_frames;
        }
        return;
    case tesla::GearDecodeStatus::Unavailable:
        if (context.telemetry.mark_selected_gear_unavailable(
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.unavailable_gear_frames;
        }
        return;
    case tesla::GearDecodeStatus::InvalidValue:
        (void)context.telemetry.mark_selected_gear_unavailable(
            decoded.source_timestamp_microseconds,
            context.received_at);
        ++context.result.rejected_gear_frames;
        return;
    case tesla::GearDecodeStatus::InvalidFrame:
        ++context.result.rejected_gear_frames;
        return;
    }
}

void ingest_turn_signal(
    IngestContext& context,
    const protocol::CanFrameView& frame) noexcept {
    const auto decoded = tesla::decode_turn_signal_lamp(frame);
    switch (decoded.status) {
    case tesla::TurnSignalDecodeStatus::Ignored:
        return;
    case tesla::TurnSignalDecodeStatus::Decoded:
        if (context.telemetry.update_turn_signal_lamp(
                application_side(decoded.side),
                decoded.lamp_illuminated,
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.decoded_turn_signal_frames;
        }
        return;
    case tesla::TurnSignalDecodeStatus::Unavailable:
        if (context.telemetry.mark_turn_signal_lamp_unavailable(
                application_side(decoded.side),
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.unavailable_turn_signal_frames;
        }
        return;
    case tesla::TurnSignalDecodeStatus::Fault:
        if (context.telemetry.mark_turn_signal_lamp_unavailable(
                application_side(decoded.side),
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.rejected_turn_signal_frames;
        }
        return;
    case tesla::TurnSignalDecodeStatus::InvalidFrame:
        ++context.result.rejected_turn_signal_frames;
        return;
    }
}

bool ingest_blind_spot(
    IngestContext& context,
    application::BlindSpotSide side,
    const tesla::BlindSpotDecodeValue& decoded,
    std::uint32_t source_timestamp_microseconds) noexcept {
    if (decoded.validity == tesla::TeslaSignalValidity::Valid) {
        return context.telemetry.update_blind_spot(
            side,
            decoded.occupied,
            source_timestamp_microseconds,
            context.received_at);
    }
    if (decoded.validity == tesla::TeslaSignalValidity::Unavailable) {
        const bool updated = context.telemetry.mark_blind_spot_unavailable(
            side,
            source_timestamp_microseconds,
            context.received_at);
        if (updated) {
            ++context.result.unavailable_driver_assistance_fields;
        }
        return updated;
    }
    return false;
}

void ingest_driver_assistance(
    IngestContext& context,
    const protocol::CanFrameView& frame) noexcept {
    const auto decoded = tesla::decode_driver_assistance(frame);
    if (decoded.status == tesla::DriverAssistanceDecodeStatus::Ignored) {
        return;
    }
    if (decoded.status == tesla::DriverAssistanceDecodeStatus::InvalidFrame) {
        ++context.result.rejected_driver_assistance_frames;
        return;
    }

    bool updated = ingest_blind_spot(
        context,
        application::BlindSpotSide::Left,
        decoded.left_blind_spot,
        decoded.source_timestamp_microseconds);
    updated = ingest_blind_spot(
                  context,
                  application::BlindSpotSide::Right,
                  decoded.right_blind_spot,
                  decoded.source_timestamp_microseconds) ||
              updated;

    switch (decoded.autopilot.validity) {
    case tesla::TeslaSignalValidity::Valid:
        updated = context.telemetry.update_autopilot_state(
                      decoded.autopilot.state,
                      decoded.source_timestamp_microseconds,
                      context.received_at) ||
                  updated;
        break;
    case tesla::TeslaSignalValidity::Unavailable:
        if (context.telemetry.mark_autopilot_unavailable(
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.unavailable_driver_assistance_fields;
            updated = true;
        }
        break;
    case tesla::TeslaSignalValidity::Fault:
        if (context.telemetry.update_autopilot_state(
                decoded.autopilot.state,
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.faulted_driver_assistance_fields;
            updated = true;
        }
        break;
    case tesla::TeslaSignalValidity::Invalid:
        ++context.result.rejected_driver_assistance_frames;
        break;
    }

    switch (decoded.speed_limit.validity) {
    case tesla::TeslaSignalValidity::Valid:
        if (context.telemetry.update_speed_limit(
                *decoded.speed_limit.value,
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.decoded_speed_limit_frames;
            updated = true;
        }
        break;
    case tesla::TeslaSignalValidity::Unavailable:
        if (context.telemetry.mark_speed_limit_unavailable(
                decoded.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.unavailable_speed_limit_frames;
            updated = true;
        }
        break;
    case tesla::TeslaSignalValidity::Fault:
    case tesla::TeslaSignalValidity::Invalid:
        ++context.result.rejected_speed_limit_frames;
        break;
    }

    if (updated) {
        ++context.result.decoded_driver_assistance_frames;
    }
}

bool ingest_exterior_lamp(
    IngestContext& context,
    application::ExteriorLightSignal signal,
    const tesla::ExteriorLampDecodeValue& decoded,
    std::uint32_t source_timestamp_microseconds) noexcept {
    if (decoded.validity == tesla::ExteriorLampValidity::Valid) {
        return context.telemetry.update_exterior_light(
            signal,
            decoded.active,
            source_timestamp_microseconds,
            context.received_at);
    }

    const bool updated = context.telemetry.mark_exterior_light_unavailable(
        signal,
        source_timestamp_microseconds,
        context.received_at);
    if (updated && decoded.validity == tesla::ExteriorLampValidity::Fault) {
        ++context.result.faulted_exterior_light_fields;
    } else if (updated) {
        ++context.result.unavailable_exterior_light_fields;
    }
    return updated;
}

void ingest_exterior_lights(
    IngestContext& context,
    const protocol::CanFrameView& frame) noexcept {
    const auto decoded = tesla::decode_exterior_lights(frame);
    bool updated = false;
    switch (decoded.status) {
    case tesla::ExteriorLightDecodeStatus::Ignored:
        return;
    case tesla::ExteriorLightDecodeStatus::InvalidFrame:
        ++context.result.rejected_exterior_light_frames;
        return;
    case tesla::ExteriorLightDecodeStatus::FrontLighting:
        updated = ingest_exterior_lamp(
            context,
            application::ExteriorLightSignal::PositionLights,
            decoded.position_lights,
            decoded.source_timestamp_microseconds);
        updated = ingest_exterior_lamp(
                      context,
                      application::ExteriorLightSignal::LowBeam,
                      decoded.low_beam,
                      decoded.source_timestamp_microseconds) ||
                  updated;
        updated = ingest_exterior_lamp(
                      context,
                      application::ExteriorLightSignal::HighBeam,
                      decoded.high_beam,
                      decoded.source_timestamp_microseconds) ||
                  updated;
        updated = ingest_exterior_lamp(
                      context,
                      application::ExteriorLightSignal::FrontFog,
                      decoded.front_fog,
                      decoded.source_timestamp_microseconds) ||
                  updated;
        break;
    case tesla::ExteriorLightDecodeStatus::FrontLightingRequests:
        updated = ingest_exterior_lamp(
            context,
            application::ExteriorLightSignal::PositionLights,
            decoded.position_lights,
            decoded.source_timestamp_microseconds);
        updated = ingest_exterior_lamp(
                      context,
                      application::ExteriorLightSignal::LowBeam,
                      decoded.low_beam,
                      decoded.source_timestamp_microseconds) ||
                  updated;
        updated = ingest_exterior_lamp(
                      context,
                      application::ExteriorLightSignal::HighBeam,
                      decoded.high_beam,
                      decoded.source_timestamp_microseconds) ||
                  updated;
        break;
    case tesla::ExteriorLightDecodeStatus::RearLighting:
        updated = ingest_exterior_lamp(
            context,
            application::ExteriorLightSignal::RearFog,
            decoded.rear_fog,
            decoded.source_timestamp_microseconds);
        break;
    case tesla::ExteriorLightDecodeStatus::LightingControls:
        updated = ingest_exterior_lamp(
            context,
            application::ExteriorLightSignal::FrontFogSwitch,
            decoded.front_fog,
            decoded.source_timestamp_microseconds);
        updated = context.telemetry.update_exterior_light(
                      application::ExteriorLightSignal::AdaptiveHighBeamEnabled,
                      decoded.adaptive_high_beam_enabled,
                      decoded.source_timestamp_microseconds,
                      context.received_at) ||
                  updated;
        break;
    }

    if (updated) {
        ++context.result.decoded_exterior_light_frames;
    }
}

void ingest_power(
    IngestContext& context,
    const protocol::CanFrameView& frame) noexcept {
    const auto power = tesla::decode_battery_power(frame);
    bool updated = false;
    switch (power.status) {
    case tesla::PowerDecodeStatus::Electrical:
        updated = context.telemetry.update_battery_electrical(
            power.electrical,
            power.source_timestamp_microseconds,
            context.received_at);
        break;
    case tesla::PowerDecodeStatus::Limits:
        updated = context.telemetry.update_battery_power_limits(
            power.limits,
            power.source_timestamp_microseconds,
            context.received_at);
        break;
    case tesla::PowerDecodeStatus::Unavailable:
        if (context.telemetry.update_battery_electrical(
                std::nullopt,
                power.source_timestamp_microseconds,
                context.received_at)) {
            ++context.result.unavailable_power_frames;
        }
        break;
    case tesla::PowerDecodeStatus::InvalidFrame:
        ++context.result.rejected_power_frames;
        break;
    case tesla::PowerDecodeStatus::Ignored:
        break;
    }
    if (updated) {
        ++context.result.decoded_power_frames;
    }
}

void ingest_frame(
    void* raw_context,
    const protocol::CanFrameView& frame) noexcept {
    auto& context = *static_cast<IngestContext*>(raw_context);
    if (context.diagnostic_capture != nullptr) {
        (void)context.diagnostic_capture->capture(
            frame.timestamp_microseconds,
            frame.bus,
            frame.address,
            frame.data,
            context.received_at);
    }
    ingest_speed(context, frame);
    ingest_gear(context, frame);
    ingest_turn_signal(context, frame);
    ingest_driver_assistance(context, frame);
    ingest_exterior_lights(context, frame);
    ingest_power(context, frame);
    const auto battery = tesla::decode_battery_charge(frame);
    bool battery_updated = false;
    switch (battery.status) {
    case tesla::BatteryChargeDecodeStatus::Energy:
    case tesla::BatteryChargeDecodeStatus::UnavailableEnergy:
        battery_updated = context.telemetry.update_battery_energy(
            battery.status == tesla::BatteryChargeDecodeStatus::Energy
                ? std::optional{battery.energy} : std::nullopt,
            battery.source_timestamp_microseconds, context.received_at);
        break;
    case tesla::BatteryChargeDecodeStatus::Buffer:
    case tesla::BatteryChargeDecodeStatus::UnavailableBuffer:
        battery_updated = context.telemetry.update_battery_buffer(
            battery.status == tesla::BatteryChargeDecodeStatus::Buffer
                ? std::optional{battery.buffer_centi_kwh} : std::nullopt,
            battery.source_timestamp_microseconds, context.received_at);
        break;
    case tesla::BatteryChargeDecodeStatus::InvalidFrame:
        ++context.result.rejected_battery_charge_frames;
        break;
    case tesla::BatteryChargeDecodeStatus::Ignored: break;
    }
    if (battery_updated) {
        if (battery.status == tesla::BatteryChargeDecodeStatus::UnavailableEnergy ||
            battery.status == tesla::BatteryChargeDecodeStatus::UnavailableBuffer)
            ++context.result.unavailable_battery_charge_frames;
        else ++context.result.decoded_battery_charge_frames;
    }
}

}  // namespace

TelemetryIngestResult DashKitTelemetryIngestor::ingest(
    std::span<const std::uint8_t> payload,
    application::TelemetryTime received_at) noexcept {
    TelemetryIngestResult result;
    IngestContext context{
        telemetry_, diagnostic_capture_, received_at, result};
    const auto parsed = protocol::parse_can_packet(
        payload,
        ingest_frame,
        &context);
    result.packet_error = parsed.error;
    return result;
}

}  // namespace tesla_headup::dashkit
