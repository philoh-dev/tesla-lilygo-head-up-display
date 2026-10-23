#include "test.hpp"
#include "tesla_headup/adapters/lvgl_rgb/rgb_frame.hpp"

#include <array>
#include <climits>
#include <limits>
#include <vector>

namespace {
using tesla_headup::platform::RgbFrame;
using tesla_headup::platform::RgbFrameExchange;
using tesla_headup::platform::RgbPlacement;

TEST_CASE("RGB viewport accepts every fitting position and rejects outside coordinates") {
    for (int y = 0; y <= 240; ++y) {
        for (int x = 0; x <= 264; ++x) {
            CHECK(RgbFrame::valid({x, y}));
        }
    }
    for (const auto position : {RgbPlacement{-1, 0}, {0, -1}, {265, 0},
                               {0, 241}, {INT_MAX, INT_MAX}, {INT_MIN, INT_MIN}}) {
        CHECK(!RgbFrame::valid(position));
    }
}

TEST_CASE("RGB composition preserves all native RGB565 pixels and blacks unused panel") {
    std::vector<std::uint16_t> logical(RgbFrame::dashboard_pixels);
    for (std::size_t i = 0; i < logical.size(); ++i) {
        logical[i] = static_cast<std::uint16_t>(i + 1);
    }
    std::vector<std::uint16_t> output(RgbFrame::pixels, 0xffff);
    for (const auto origin : {RgbPlacement{0, 0}, {264, 240}, {132, 0}, {17, 33}}) {
        for (const bool mirrored : {false, true}) {
            CHECK(RgbFrame::compose(logical, output, origin, mirrored));
            for (int y = 0; y < RgbFrame::height; ++y) {
                for (int x = 0; x < RgbFrame::width; ++x) {
                    const int source_x = (mirrored ? 799 - x : x) - origin.x;
                    const int source_y = y - origin.y;
                    const std::uint16_t expected = source_x >= 0 && source_x < 536 &&
                            source_y >= 0 && source_y < 240
                        ? logical[source_y * 536 + source_x] : 0;
                    CHECK_EQ(output[y * 800 + x], expected);
                }
            }
        }
    }
}

TEST_CASE("RGB relocation clears previously occupied pixels in either framebuffer") {
    std::vector<std::uint16_t> logical(RgbFrame::dashboard_pixels, 0xf800);
    std::vector<std::uint16_t> output(RgbFrame::pixels, 0xffff);
    CHECK(RgbFrame::compose(logical, output, {0, 0}, false));
    CHECK(RgbFrame::compose(logical, output, {264, 240}, true));
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 800; ++x) {
            CHECK_EQ(output[y * 800 + x], 0);
        }
    }
    CHECK_EQ(output[240 * 800], 0xf800);
    CHECK_EQ(output[240 * 800 + 536], 0);
}

TEST_CASE("RGB partial flushes preserve previous rectangles without byte swapping") {
    std::vector<std::uint16_t> logical(RgbFrame::dashboard_pixels, 0);
    const std::array<std::uint16_t, 4> colors{0xf800, 0x07e0, 0x001f, 0xffff};
    const auto bytes = std::span{reinterpret_cast<const std::uint8_t*>(colors.data()),
                                 colors.size() * 2U};
    CHECK(RgbFrame::update(logical, 3, 4, 4, 5, bytes));
    CHECK(RgbFrame::update(logical, 534, 238, 535, 239, bytes));
    CHECK_EQ(logical[4 * 536 + 3], 0xf800);
    CHECK_EQ(logical[4 * 536 + 4], 0x07e0);
    CHECK_EQ(logical[5 * 536 + 3], 0x001f);
    CHECK_EQ(logical[5 * 536 + 4], 0xffff);
    CHECK_EQ(logical.back(), 0xffff);
    CHECK_EQ(logical.front(), 0);
}

TEST_CASE("RGB frame functions reject malformed buffers and rectangles without writes") {
    std::vector<std::uint16_t> logical(RgbFrame::dashboard_pixels, 0x1234);
    std::vector<std::uint16_t> output(RgbFrame::pixels, 0xabcd);
    const std::array<std::uint8_t, 2> pixel{0, 0};
    CHECK(!RgbFrame::compose(logical, output, {-1, 0}, false));
    CHECK(!RgbFrame::compose(std::span{logical}.first(1), output, {}, false));
    CHECK(!RgbFrame::compose(logical, std::span{output}.first(1), {}, false));
    CHECK(!RgbFrame::compose(std::span{output}.first(RgbFrame::dashboard_pixels), output, {}, false));
    for (const auto area : {std::array{-1, 0, 0, 0}, {0, -1, 0, 0},
                           {3, 0, 2, 0}, {0, 3, 0, 2}, {0, 0, 536, 0},
                           {0, 0, 0, 240}, {0, 0, INT_MAX, INT_MAX}}) {
        CHECK(!RgbFrame::update(logical, area[0], area[1], area[2], area[3], pixel));
    }
    CHECK(!RgbFrame::update(logical, 0, 0, 1, 1, pixel));
    CHECK(!RgbFrame::update(std::span{logical}.first(1), 0, 0, 0, 0, pixel));
    CHECK(std::all_of(logical.begin(), logical.end(), [](auto p) { return p == 0x1234; }));
    CHECK(std::all_of(output.begin(), output.end(), [](auto p) { return p == 0xabcd; }));
}

TEST_CASE("RGB framebuffer ownership waits for a post-submission completion") {
    RgbFrameExchange exchange;
    CHECK_EQ(exchange.writable_index(), 1U); // Driver starts on buffer 0.
    CHECK(!exchange.retire(41));
    exchange.submitted(42);
    CHECK(exchange.pending());
    CHECK(!exchange.retire(42));
    CHECK(!exchange.retire(43)); // First callback may have latched the old FB on the other core.
    CHECK(exchange.pending());
    CHECK(exchange.retire(44));
    CHECK(!exchange.pending());
    CHECK_EQ(exchange.writable_index(), 0U);
    CHECK(!exchange.retire(44));
    exchange.submitted(44);
    CHECK(!exchange.retire(44));
    CHECK(exchange.retire(48)); // Coalesced interrupts are safe.
    CHECK_EQ(exchange.writable_index(), 1U);
}

TEST_CASE("RGB framebuffer completion counter handles uint32 rollover") {
    RgbFrameExchange exchange;
    exchange.submitted(std::numeric_limits<std::uint32_t>::max());
    CHECK(!exchange.retire(0));
    CHECK(exchange.retire(1));
    CHECK_EQ(exchange.writable_index(), 0U);
}

TEST_CASE("full-panel RGB surfaces validate dimensions partial bottom-right updates and atomic rejection") {
    const tesla_headup::platform::RgbSurface full{800, 480};
    std::vector<std::uint16_t> logical(full.pixels(), 0), output(full.pixels(), 0);
    const std::array<std::uint8_t, 2> pixel{0x00, 0xf8};
    CHECK(RgbFrame::valid({0, 0}, full));
    CHECK(!RgbFrame::valid({1, 0}, full));
    CHECK(!RgbFrame::valid({0, 1}, full));
    for (const auto size : {tesla_headup::platform::RgbSurface{0, 480}, {-1, 480}, {801, 480}, {800, INT_MAX}}) {
        CHECK(!RgbFrame::valid({0, 0}, size));
        CHECK(!RgbFrame::update(logical, 0, 0, 0, 0, pixel, size));
        CHECK(!RgbFrame::compose(logical, output, {0, 0}, false, size));
    }
    CHECK(RgbFrame::update(logical, 799, 479, 799, 479, pixel, full));
    CHECK_EQ(logical.back(), 0xf800);
    CHECK(!RgbFrame::update(logical, 800, 479, 800, 479, pixel, full));
    CHECK(RgbFrame::compose(logical, output, {0, 0}, true, full));
    CHECK_EQ(output[479 * 800], 0xf800);
    CHECK_EQ(output.back(), 0);
}
}  // namespace
