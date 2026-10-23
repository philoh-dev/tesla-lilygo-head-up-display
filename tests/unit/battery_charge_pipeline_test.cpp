#include "test.hpp"
#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/adapters/tesla_decoder/tesla_battery_charge_decoder.hpp"
#include "tesla_headup/application/hud_runtime/battery_charge_presenter.hpp"

#include <array>
#include <chrono>
#include <limits>
#include <vector>

namespace {
using namespace std::chrono_literals;
using tesla_headup::domain::BatteryCharge;
using tesla_headup::application::TelemetryStore;
using tesla_headup::tesla::BatteryChargeDecodeStatus;
using tesla_headup::tesla::decode_battery_charge;

// Independent hand-authored bytes: bus 1, 0x352, mux0 79/41.76 kWh,
// mux1 3 kWh. Pinned DBC and calculation provenance: docs/dashkit-integration.md.
constexpr std::array<std::uint8_t, 8> energy{0, 0, 0x6E, 0x0F, 0x28, 0x08, 0, 0};
constexpr std::array<std::uint8_t, 8> buffer{1, 0, 0x2C, 0x01, 0, 0, 0, 0};

std::vector<std::uint8_t> battery_packet() {
    return {2,
        123, 0, 0, 0, 1, 0x52, 3, 0, 0, 8, 0, 0, 0x6E, 0x0F, 0x28, 0x08, 0, 0,
        124, 0, 0, 0, 1, 0x52, 3, 0, 0, 8, 1, 0, 0x2C, 0x01, 0, 0, 0, 0};
}

class BatteryCanvas final : public tesla_headup::application::DashboardCanvas {
public:
    void begin_frame() override { percent.reset(); }
    void draw_text(const tesla_headup::application::TextElement&) override {}
    void draw_symbol(const tesla_headup::application::SymbolElement&) override {}
    void draw_edge_gradient(const tesla_headup::application::EdgeGradientElement&) override {}
    void draw_battery(const tesla_headup::application::BatteryChargeElement& value) override { percent = value.percent; }
    void end_frame() override {}
    std::optional<unsigned> percent;
};

TEST_CASE("battery charge floors buffered energy and clamps without losing zero") {
    for (unsigned value = 0; value <= 100; ++value)
        CHECK_EQ(BatteryCharge::from_percent(value)->percent(), value);
    CHECK(!BatteryCharge::from_percent(101));
    CHECK(!BatteryCharge::from_percent(std::numeric_limits<unsigned>::max()));
    CHECK_EQ(BatteryCharge::from_energy({7900, 4176}, 300)->percent(), 51);
    CHECK_EQ(BatteryCharge::from_energy({10000, 5050}, 0)->percent(), 50);
    CHECK_EQ(BatteryCharge::from_energy({10000, 6000}, 0)->percent(), 60);
    CHECK_EQ(BatteryCharge::from_energy({10000, 6049}, 0)->percent(), 60);
    CHECK_EQ(BatteryCharge::from_energy({10000, 6050}, 0)->percent(), 60);
    CHECK_EQ(BatteryCharge::from_energy({10000, 6099}, 0)->percent(), 60);
    CHECK_EQ(BatteryCharge::from_energy({10000, 6100}, 0)->percent(), 61);
    CHECK_EQ(BatteryCharge::from_energy({10300, 6399}, 300)->percent(), 60);
    CHECK_EQ(BatteryCharge::from_energy({10300, 6400}, 300)->percent(), 61);
    CHECK_EQ(BatteryCharge::from_energy({10000, 99}, 0)->percent(), 0);
    CHECK_EQ(BatteryCharge::from_energy({10000, 9999}, 0)->percent(), 99);
    CHECK_EQ(BatteryCharge::from_energy({10000, 10000}, 0)->percent(), 100);
    CHECK_EQ(BatteryCharge::from_energy({7900, 0}, 300)->percent(), 0);
    CHECK_EQ(BatteryCharge::from_energy({7900, 8000}, 300)->percent(), 100);
    CHECK(!BatteryCharge::from_energy({0, 0}, 0));
    CHECK(!BatteryCharge::from_energy({300, 300}, 300));
    CHECK(!BatteryCharge::from_energy({299, 300}, 300));
    CHECK_EQ(BatteryCharge::from_energy({1, UINT32_MAX}, 0)->percent(), 100);
}

TEST_CASE("battery floor stays within exact quotient bounds for small energy ratios") {
    for (unsigned available = 1; available <= 257; ++available) {
        for (unsigned remaining = 0; remaining <= available; ++remaining) {
            const auto percent = BatteryCharge::from_energy(
                {available + 37, remaining + 37}, 37)->percent();
            // Exact floor invariant, including denominators not divisible by 100.
            CHECK(percent * available <= remaining * 100U);
            CHECK((percent + 1U) * available > remaining * 100U);
        }
    }
}

TEST_CASE("battery packet presentation floors fractions and only renders whole percent changes") {
    TelemetryStore telemetry;
    tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};
    BatteryCanvas canvas;
    tesla_headup::application::BatteryChargeIndicator feature;
    tesla_headup::application::Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature));
    tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::BatteryChargePresenter presenter{telemetry, feature, scheduler};
    auto packet = battery_packet();
    // Full 79 kWh, buffer 3 kWh, remaining 48.98 kWh => exactly 60.5%.
    packet[15] = 0x91; packet[16] = 0x09;
    CHECK_EQ(ingestor.ingest(packet, 0ms).decoded_battery_charge_frames, 2U);
    CHECK(presenter.refresh(0ms));
    CHECK(scheduler.tick(0ms));
    CHECK_EQ(*canvas.percent, 60U);
    // Remaining 49.34 kWh => 60.973...%, still 60 without a redundant frame.
    packet[15] = 0xA3;
    CHECK_EQ(ingestor.ingest(packet, 100ms).decoded_battery_charge_frames, 2U);
    CHECK(!presenter.refresh(100ms));
    // Remaining 49.36 kWh => exactly 61%.
    packet[15] = 0xA4;
    CHECK_EQ(ingestor.ingest(packet, 200ms).decoded_battery_charge_frames, 2U);
    CHECK(presenter.refresh(200ms));
    CHECK(scheduler.tick(200ms));
    CHECK_EQ(*canvas.percent, 61U);
}

TEST_CASE("battery decoder validates bus DLC mux and documented unavailable codes") {
    const auto decoded = decode_battery_charge({42, 1, 850, energy});
    CHECK_EQ(decoded.status, BatteryChargeDecodeStatus::Energy);
    CHECK_EQ(decoded.energy.full_centi_kwh, 7900U);
    CHECK_EQ(decoded.energy.remaining_centi_kwh, 4176U);
    CHECK_EQ(decoded.source_timestamp_microseconds, 42U);
    CHECK_EQ(decode_battery_charge({43, 1, 850, buffer}).buffer_centi_kwh, 300U);
    for (std::size_t size = 0; size < 8; ++size)
        CHECK_EQ(decode_battery_charge({0, 1, 850, std::span{energy}.first(size)}).status,
                 BatteryChargeDecodeStatus::InvalidFrame);
    std::array<std::uint8_t, 9> oversized{};
    CHECK_EQ(decode_battery_charge({0, 1, 850, oversized}).status, BatteryChargeDecodeStatus::InvalidFrame);
    for (std::uint8_t bus : {0, 2, 255})
        CHECK_EQ(decode_battery_charge({0, bus, 850, energy}).status, BatteryChargeDecodeStatus::Ignored);
    CHECK_EQ(decode_battery_charge({0, 1, 849, energy}).status, BatteryChargeDecodeStatus::Ignored);
    for (unsigned first = 0; first < 256; ++first) {
        auto data = energy;
        data[0] = static_cast<std::uint8_t>(first);
        const auto status = decode_battery_charge({0, 1, 850, data}).status;
        CHECK_EQ(status, (first & 3U) == 0 ? BatteryChargeDecodeStatus::Energy
            : (first & 3U) == 1 ? BatteryChargeDecodeStatus::Buffer : BatteryChargeDecodeStatus::Ignored);
    }
    for (unsigned field : {2U, 4U}) {
        auto unavailable = energy;
        unavailable[field] = 0xFF; unavailable[field + 1] = 0x07;
        CHECK_EQ(decode_battery_charge({0, 1, 850, unavailable}).status,
                 BatteryChargeDecodeStatus::UnavailableEnergy);
    }
    auto unavailable = buffer;
    unavailable[2] = 0xFF; unavailable[3] = 0;
    CHECK_EQ(decode_battery_charge({0, 1, 850, unavailable}).status,
             BatteryChargeDecodeStatus::UnavailableBuffer);
}

TEST_CASE("battery telemetry requires independent fresh ordered energy and buffer observations") {
    TelemetryStore store;
    CHECK(!store.battery_charge(0ms));
    CHECK(store.update_battery_energy(tesla_headup::domain::BatteryEnergy{7900, 4176}, 1, 0ms));
    CHECK(!store.battery_charge(0ms));
    CHECK(store.update_battery_buffer(300, 2, 100ms));
    CHECK_EQ(store.battery_charge(5000ms)->percent(), 51);
    CHECK(!store.battery_charge(5001ms));
    CHECK(store.update_battery_energy(tesla_headup::domain::BatteryEnergy{7900, 4176}, 3, 5001ms));
    CHECK_EQ(store.battery_charge(5100ms)->percent(), 51);
    CHECK(!store.battery_charge(5101ms));
    CHECK(!store.update_battery_energy(std::nullopt, 4, 5000ms));
    CHECK(!store.update_battery_buffer(std::nullopt, 4, 99ms));
    CHECK(store.update_battery_buffer(300, 4, 5101ms));
    CHECK(store.update_battery_energy(std::nullopt, 5, 5102ms));
    CHECK(!store.battery_charge(5102ms));
    CHECK(store.update_battery_energy(tesla_headup::domain::BatteryEnergy{7900, 0}, 6, 5103ms));
    CHECK_EQ(store.battery_charge(5103ms)->percent(), 0);
}

TEST_CASE("battery decoder exhausts every raw energy and buffer field without inferred SNA codes") {
    for (unsigned raw = 0; raw <= 65535; ++raw) {
        for (const auto field : {2U, 4U}) {
            auto data = energy;
            data[field] = static_cast<std::uint8_t>(raw);
            data[field + 1] = static_cast<std::uint8_t>(raw >> 8U);
            const auto decoded = decode_battery_charge({0, 1, 850, data});
            CHECK_EQ(decoded.status, raw == 2047 ? BatteryChargeDecodeStatus::UnavailableEnergy
                                                 : BatteryChargeDecodeStatus::Energy);
            CHECK_EQ(field == 2 ? decoded.energy.full_centi_kwh
                                : decoded.energy.remaining_centi_kwh, raw * 2U);
        }
        auto data = buffer;
        data[2] = static_cast<std::uint8_t>(raw);
        data[3] = static_cast<std::uint8_t>(raw >> 8U);
        const auto decoded = decode_battery_charge({0, 1, 850, data});
        CHECK_EQ(decoded.status, raw == 255 ? BatteryChargeDecodeStatus::UnavailableBuffer
                                            : BatteryChargeDecodeStatus::Buffer);
        CHECK_EQ(decoded.buffer_centi_kwh, raw);
    }
}

TEST_CASE("battery DashKit replay is atomic expires and respects disabled feature") {
    TelemetryStore telemetry;
    tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};
    BatteryCanvas canvas;
    tesla_headup::application::BatteryChargeIndicator feature;
    tesla_headup::application::Dashboard dashboard{canvas};
    CHECK(dashboard.add_feature(feature));
    tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
    tesla_headup::application::BatteryChargePresenter presenter{telemetry, feature, scheduler};
    CHECK(scheduler.tick(0ms));
    CHECK(!canvas.percent);
    auto truncated = battery_packet(); truncated.pop_back();
    CHECK(!ingestor.ingest(truncated, 10ms).accepted_packet());
    CHECK(!telemetry.battery_charge(10ms));
    CHECK_EQ(ingestor.ingest(battery_packet(), 20ms).decoded_battery_charge_frames, 2U);
    CHECK(presenter.refresh(20ms));
    CHECK(scheduler.tick(40ms));
    CHECK_EQ(*canvas.percent, 51U);
    CHECK(!presenter.refresh(100ms));
    CHECK(!presenter.refresh(5020ms));
    CHECK(presenter.refresh(5021ms));
    CHECK(scheduler.tick(5021ms));
    CHECK(!canvas.percent);
    CHECK(dashboard.set_enabled(tesla_headup::application::FeatureId::BatteryCharge, false));
    CHECK(ingestor.ingest(battery_packet(), 5100ms).accepted_packet());
    CHECK(presenter.refresh(5100ms));
    CHECK(!dashboard.is_enabled(tesla_headup::application::FeatureId::BatteryCharge));
    CHECK(scheduler.tick(5100ms));
    CHECK(!canvas.percent);
}
}  // namespace
