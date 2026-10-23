#include "test.hpp"

#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/adapters/tesla_decoder/tesla_power_decoder.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/hud_runtime/power_presenter.hpp"
#include "tesla_headup/application/hud_runtime/display_self_test_source.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace {
using namespace std::chrono_literals;
using tesla_headup::application::TelemetryStore;
using tesla_headup::dashkit::protocol::CanFrameView;
using tesla_headup::domain::PowerFlow;
using tesla_headup::tesla::PowerDecodeStatus;
using tesla_headup::tesla::decode_battery_power;

TEST_CASE("power utilization uses battery watts and each directional limit") {
    CHECK_EQ(PowerFlow::from_battery({40000, 1000}, {8000, 4000})->permille(), 500);
    CHECK_EQ(PowerFlow::from_battery({40000, -500}, {8000, 4000})->permille(), -500);
    CHECK_EQ(PowerFlow::from_battery({40000, 0}, {8000, 4000})->permille(), 0);
    CHECK_EQ(PowerFlow::from_battery({40000, 3000}, {8000, 4000})->permille(), 1000);
    CHECK_EQ(PowerFlow::from_battery({40000, -3000}, {8000, 4000})->permille(), -1000);
    CHECK_EQ(PowerFlow::from_battery({65535, 32767}, {1, 1})->permille(), 1000);
    CHECK_EQ(PowerFlow::from_battery({65535, -32768}, {1, 1})->permille(), -1000);
    CHECK(!PowerFlow::from_battery({0, 1000}, {8000, 4000}));
    CHECK(!PowerFlow::from_battery({40000, 1000}, {0, 4000}));
    CHECK(!PowerFlow::from_battery({40000, -500}, {8000, 0}));
    // A zero recuperation limit must not suppress valid discharge output.
    CHECK_EQ(PowerFlow::from_battery({40000, 1000}, {8000, 0})->permille(), 500);
}

TEST_CASE("BMS decoder covers every signed 15-bit current and unavailable code") {
    std::array<std::uint8_t, 8> payload{0x40, 0x9C}; // 400 V
    for (std::uint32_t raw = 0; raw < 0x8000U; ++raw) {
        payload[2] = static_cast<std::uint8_t>(raw);
        // Bit 31 belongs to another field and must never affect current.
        payload[3] = static_cast<std::uint8_t>((raw >> 8U) | 0x80U);
        const auto result = decode_battery_power({123, 1, 306, payload});
        CHECK_EQ(result.source_timestamp_microseconds, 123U);
        if (raw == 0x4000U) {
            CHECK_EQ(result.status, PowerDecodeStatus::Unavailable);
        } else {
            CHECK_EQ(result.status, PowerDecodeStatus::Electrical);
            const auto signed_raw = raw < 0x4000U
                ? static_cast<std::int32_t>(raw)
                : static_cast<std::int32_t>(raw) - 0x8000;
            CHECK_EQ(result.electrical.current_deciamps, -signed_raw);
            CHECK_EQ(result.electrical.voltage_centivolts, 40000U);
        }
    }
    payload[0] = 0;
    payload[1] = 0;
    CHECK_EQ(decode_battery_power({123, 1, 306, payload}).status,
             PowerDecodeStatus::Unavailable);
}

TEST_CASE("power decoder validates bus identifier and required signal bytes") {
    const std::array<std::uint8_t, 8> data{0x40, 0x1F, 0x00, 0x7D};
    const auto limits = decode_battery_power({42, 1, 594, data});
    CHECK_EQ(limits.status, PowerDecodeStatus::Limits);
    CHECK_EQ(limits.limits.regeneration_centikw, 8000U);
    CHECK_EQ(limits.limits.discharge_centikw, 32000U);
    for (const std::uint32_t address : {306U, 594U}) {
        for (std::size_t length = 0; length < 4; ++length) {
            CHECK_EQ(decode_battery_power(
                {1, 1, address, std::span{data}.first(length)}).status,
                PowerDecodeStatus::InvalidFrame);
        }
        for (std::size_t length = 4; length <= data.size(); ++length) {
            CHECK_EQ(decode_battery_power(
                {1, 1, address, std::span{data}.first(length)}).status,
                address == 306 ? PowerDecodeStatus::Electrical : PowerDecodeStatus::Limits);
        }
        const std::array<std::uint8_t, 9> oversized{};
        CHECK_EQ(decode_battery_power({1, 1, address, oversized}).status,
                 PowerDecodeStatus::InvalidFrame);
        for (const std::uint8_t bus : {0U, 2U}) {
            CHECK_EQ(decode_battery_power({1, bus, address, data}).status,
                     PowerDecodeStatus::Ignored);
        }
    }
    CHECK_EQ(decode_battery_power({1, 1, 305, data}).status,
             PowerDecodeStatus::Ignored);
    const std::array<std::uint8_t, 4> sna{0x40, 0x9C, 0, 0x40};
    CHECK_EQ(decode_battery_power({1, 1, 306, sna}).status,
             PowerDecodeStatus::Unavailable);
    const std::array<std::uint8_t, 4> no_voltage{0, 0, 0x60, 0x70};
    CHECK_EQ(decode_battery_power({1, 1, 306, no_voltage}).status,
             PowerDecodeStatus::Unavailable);
}

TEST_CASE("power telemetry expires electrical data and limits independently") {
    TelemetryStore store{{.power_limits_timeout = 500ms}};
    CHECK(!store.power_flow(0ms));
    CHECK(store.update_battery_power_limits({32000, 8000}, 1, 0ms));
    CHECK(!store.power_flow(0ms));
    CHECK(store.update_battery_electrical(
        tesla_headup::domain::BatteryElectricalState{40000, 2000}, 2, 100ms));
    CHECK_EQ(store.power_flow(500ms)->permille(), 250);
    CHECK(!store.power_flow(501ms)); // Old limits, still-fresh electrical data.
    CHECK(store.update_battery_power_limits({32000, 8000}, 3, 501ms));
    CHECK_EQ(store.power_flow(600ms)->permille(), 250);
    CHECK(!store.power_flow(601ms)); // Electrical data now expires.
    CHECK(!store.update_battery_power_limits({1, 1}, 4, 500ms));
    CHECK(!store.update_battery_electrical(std::nullopt, 5, 99ms));
    CHECK(store.update_battery_electrical(std::nullopt, 6, 602ms));
    CHECK(!store.power_flow(602ms));
}

TEST_CASE("power retains limits across bounded delivery gaps without retaining stale current") {
    TelemetryStore store;
    CHECK(store.update_battery_power_limits({32000, 8000}, 1, 0ms));
    CHECK(store.update_battery_electrical(
        tesla_headup::domain::BatteryElectricalState{40000, 2000}, 2, 600ms));
    CHECK(store.power_flow(600ms));
    CHECK_EQ(store.power_flow(600ms)->permille(), 250);
    CHECK(store.power_flow(1100ms));
    CHECK(!store.power_flow(1101ms)); // Current still expires after 500 ms.
    CHECK(store.update_battery_electrical(
        tesla_headup::domain::BatteryElectricalState{40000, -1000}, 3, 1490ms));
    CHECK(store.power_flow(1500ms));
    CHECK_EQ(store.power_flow(1500ms)->permille(), -500);
    CHECK(!store.power_flow(1501ms)); // Limits have their own finite deadline.
    CHECK(store.update_battery_power_limits({32000, 8000}, 4, 1502ms));
    CHECK(store.power_flow(1502ms));
    CHECK(store.update_battery_electrical(std::nullopt, 5, 1503ms));
    CHECK(!store.power_flow(1503ms)); // Explicit unavailable never gets held.
}

class PowerCanvas final : public tesla_headup::application::DashboardCanvas {
public:
    std::optional<std::int16_t> level;
    void begin_frame() override { level.reset(); }
    void end_frame() override {}
    void draw_text(const tesla_headup::application::TextElement&) override {}
    void draw_symbol(const tesla_headup::application::SymbolElement&) override {}
    void draw_edge_gradient(const tesla_headup::application::EdgeGradientElement&) override {}
    void draw_power(const tesla_headup::application::PowerBarElement& element) override {
        level = element.permille;
    }
};

std::vector<std::uint8_t> power_packet(bool regeneration, std::size_t length = 8) {
    // BMS_powerAvailable: 80 kW regen, 320 kW discharge; BMS_hvBusStatus:
    // 400 V and either +400 A discharge or -100 A recuperation.
    std::vector<std::uint8_t> packet{2};
    const std::array<std::uint8_t, 8> limits{0x40, 0x1F, 0x00, 0x7D};
    const std::array<std::uint8_t, 8> electrical{
        0x40, 0x9C,
        static_cast<std::uint8_t>(regeneration ? 0xE8 : 0x60),
        static_cast<std::uint8_t>(regeneration ? 0x03 : 0x70)};
    const auto append = [&packet, length](std::uint32_t address, const auto& payload) {
        packet.insert(packet.end(), {123, 0, 0, 0, 1,
            static_cast<std::uint8_t>(address),
            static_cast<std::uint8_t>(address >> 8U), 0, 0,
            static_cast<std::uint8_t>(length)});
        packet.insert(packet.end(), payload.begin(), payload.begin() + length);
    };
    append(594, limits);
    append(306, electrical);
    return packet;
}

TEST_CASE("four through eight byte power packets drive both gauge directions") {
    for (std::size_t length = 4; length <= 8; ++length) {
        TelemetryStore telemetry;
        tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};
        PowerCanvas canvas;
        tesla_headup::application::PowerIndicator feature;
        tesla_headup::application::Dashboard dashboard{canvas};
        CHECK(dashboard.add_feature(feature));
        tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
        tesla_headup::application::PowerPresenter presenter{telemetry, feature, scheduler};
        CHECK_EQ(ingestor.ingest(power_packet(false, length), 0ms).decoded_power_frames, 2U);
        CHECK(presenter.refresh(0ms));
        CHECK(scheduler.tick(0ms));
        CHECK_EQ(*canvas.level, 500);
        CHECK_EQ(ingestor.ingest(power_packet(true, length), 50ms).decoded_power_frames, 2U);
        CHECK(presenter.refresh(50ms));
        CHECK(scheduler.tick(50ms));
        CHECK_EQ(*canvas.level, -500);
        auto malformed = power_packet(false, length);
        malformed.pop_back();
        CHECK(!ingestor.ingest(malformed, 60ms).accepted_packet());
        CHECK_EQ(telemetry.power_flow(60ms)->permille(), -500);
    }
}

TEST_CASE("separately delivered limit and current packets preserve bounded live gauge updates") {
    TelemetryStore telemetry;
    tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};
    PowerCanvas canvas;
    tesla_headup::application::PowerIndicator feature;
    tesla_headup::application::Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature));
    tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::PowerPresenter presenter{telemetry, feature, scheduler};
    const auto packet = power_packet(false, 4);
    std::vector<std::uint8_t> limits{1}, electrical{1};
    limits.insert(limits.end(), packet.begin() + 1, packet.begin() + 15);
    electrical.insert(electrical.end(), packet.begin() + 15, packet.end());
    CHECK_EQ(ingestor.ingest(limits, 0ms).decoded_power_frames, 1U);
    CHECK(!presenter.refresh(0ms)); // No measured current: never invent a fill.
    CHECK_EQ(ingestor.ingest(electrical, 600ms).decoded_power_frames, 1U);
    CHECK(presenter.refresh(600ms));
    CHECK(scheduler.tick(600ms));
    CHECK_EQ(*canvas.level, 500);
    CHECK_EQ(ingestor.ingest(electrical, 1490ms).decoded_power_frames, 1U);
    CHECK(!presenter.refresh(1500ms));
    CHECK(presenter.refresh(1501ms)); // Current alone cannot renew limits.
    CHECK(scheduler.tick(1501ms));
    CHECK(!canvas.level);
    CHECK_EQ(ingestor.ingest(limits, 1502ms).decoded_power_frames, 1U);
    CHECK(presenter.refresh(1502ms));
    CHECK(scheduler.tick(1541ms));
    CHECK_EQ(*canvas.level, 500);
    CHECK(presenter.refresh(1991ms)); // Limits alone cannot renew current.
    CHECK(scheduler.tick(1991ms));
    CHECK(!canvas.level);
}

TEST_CASE("DashKit power packets reach the rail and fail quiet after silence") {
    TelemetryStore telemetry;
    tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};
    PowerCanvas canvas;
    tesla_headup::application::PowerIndicator feature;
    tesla_headup::application::Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature));
    tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::PowerPresenter presenter{telemetry, feature, scheduler};
    CHECK(scheduler.tick(0ms));
    CHECK(!canvas.level);

    CHECK_EQ(ingestor.ingest(power_packet(false), 10ms).decoded_power_frames, 2U);
    CHECK(presenter.refresh(10ms));
    CHECK(scheduler.tick(40ms));
    CHECK_EQ(*canvas.level, 500);
    CHECK(!presenter.refresh(50ms));
    CHECK_EQ(ingestor.ingest(power_packet(true), 60ms).decoded_power_frames, 2U);
    CHECK(presenter.refresh(60ms));
    CHECK(scheduler.tick(80ms));
    CHECK_EQ(*canvas.level, -500);
    auto truncated = power_packet(false);
    truncated.pop_back();
    CHECK(!ingestor.ingest(truncated, 100ms).accepted_packet());
    CHECK(!presenter.refresh(100ms));
    CHECK_EQ(telemetry.power_flow(100ms)->permille(), -500);
    auto unavailable = power_packet(true);
    // Electrical frame is last: clear bits 16..30 to the raw SNA code 0x4000.
    unavailable[unavailable.size() - 6] = 0;
    unavailable[unavailable.size() - 5] = 0x40;
    CHECK_EQ(ingestor.ingest(unavailable, 110ms).unavailable_power_frames, 1U);
    CHECK(presenter.refresh(110ms));
    CHECK(scheduler.tick(120ms));
    CHECK(!canvas.level);
    CHECK_EQ(ingestor.ingest(power_packet(true), 130ms).decoded_power_frames, 2U);
    CHECK(presenter.refresh(130ms));
    CHECK(scheduler.tick(160ms));
    CHECK_EQ(*canvas.level, -500);
    CHECK(!presenter.refresh(630ms));
    CHECK(presenter.refresh(631ms));
    CHECK(scheduler.tick(631ms));
    CHECK(!canvas.level);
}

TEST_CASE("display self-test exercises discharge and recuperation without BLE") {
    TelemetryStore telemetry;
    tesla_headup::application::DisplaySelfTestSource source{telemetry};
    CHECK(source.tick(0ms));
    CHECK(!telemetry.power_flow(0ms));
    CHECK(source.tick(8s));
    CHECK(telemetry.power_flow(8s)->permille() > 0);
    CHECK(source.tick(30s));
    CHECK(telemetry.power_flow(30s)->permille() < 0);
    CHECK(source.tick(67'500ms));
    CHECK_EQ(telemetry.power_flow(67'500ms)->permille(), 500);
    CHECK(source.tick(69s));
    CHECK(!telemetry.power_flow(69s));
    CHECK(source.tick(70s));
    CHECK_EQ(telemetry.power_flow(70s)->permille(), -500);
    CHECK(source.tick(71'500ms));
    CHECK(!telemetry.power_flow(71'500ms));
}

}  // namespace
