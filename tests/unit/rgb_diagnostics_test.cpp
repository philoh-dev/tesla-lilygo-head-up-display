#include "test.hpp"
#include "tesla_headup/adapters/lvgl_rgb/rgb_diagnostics.hpp"

#include <climits>
#include <limits>
#include <vector>

namespace {
using namespace tesla_headup::platform;

TEST_CASE("diagnostic solid frames contain only the exact native RGB565 primary color") {
    std::vector<std::uint16_t> frame(RgbFrame::pixels);
    const std::array colors{0xf800, 0x07e0, 0x001f, 0xffff, 0x0000};
    for (unsigned i = 0; i < colors.size(); ++i) {
        CHECK(RgbDiagnostics::render(frame, static_cast<RgbDiagnosticPattern>(i), {}, false));
        CHECK(std::all_of(frame.begin(), frame.end(), [color = colors[i]](auto p) { return p == color; }));
    }
}

TEST_CASE("diagnostic checkerboard has exact 20 pixel tiles across every panel pixel") {
    std::vector<std::uint16_t> frame(RgbFrame::pixels);
    CHECK(RgbDiagnostics::render(frame, RgbDiagnosticPattern::Checkerboard, {}, false));
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 800; ++x)
            CHECK_EQ(frame[y * 800 + x], ((x / 20 + y / 20) & 1) ? 0xffff : 0);
}

TEST_CASE("diagnostic corners contain all edge pixels corner labels and ordered primary swatches") {
    std::vector<std::uint16_t> frame(RgbFrame::pixels);
    CHECK(RgbDiagnostics::render(frame, RgbDiagnosticPattern::Corners, {}, false));
    for (int x = 0; x < 800; ++x) {
        CHECK_EQ(frame[x], 0xffff); CHECK_EQ(frame[479 * 800 + x], 0xffff);
    }
    for (int y = 0; y < 480; ++y) {
        CHECK_EQ(frame[y * 800], 0xffff); CHECK_EQ(frame[y * 800 + 799], 0xffff);
    }
    for (const auto position : {std::array{8, 8}, {624, 8}, {8, 448}, {588, 448}}) {
        int white = 0;
        for (int y = position[1]; y < position[1] + 21; ++y)
            for (int x = position[0]; x < position[0] + 36; ++x)
                if (frame[y * 800 + x] == 0xffff) ++white;
        CHECK(white > 0);
    }
    CHECK_EQ(frame[290 * 800 + 300], 0xf800);
    CHECK_EQ(frame[290 * 800 + 400], 0x07e0);
    CHECK_EQ(frame[290 * 800 + 500], 0x001f);
}

TEST_CASE("diagnostic viewport borders fit all corners and do not write outside their logical box") {
    std::vector<std::uint16_t> guarded(RgbFrame::pixels + 2, 0xabcd);
    auto frame = std::span{guarded}.subspan(1, RgbFrame::pixels);
    for (const auto origin : {RgbPlacement{0, 0}, {264, 240}, {132, 0}, {17, 33}, {132, 120}}) {
        CHECK(RgbDiagnostics::render(frame, RgbDiagnosticPattern::Viewport, origin, false));
        for (int y = 0; y < 480; ++y) {
            for (int x = 0; x < 800; ++x) {
                const bool inside = x >= origin.x && x < origin.x + 536 && y >= origin.y && y < origin.y + 240;
                const bool border = inside && (x < origin.x + 2 || x >= origin.x + 534 ||
                                               y < origin.y + 2 || y >= origin.y + 238);
                if (border) CHECK_EQ(frame[y * 800 + x], 0x07ff);
                if (!inside) CHECK_EQ(frame[y * 800 + x], x == 0 || y == 0 || x == 799 || y == 479 ? 0xffff : 0);
            }
        }
        CHECK_EQ(guarded.front(), 0xabcd); CHECK_EQ(guarded.back(), 0xabcd);
    }
}

TEST_CASE("every diagnostic is mirrored exactly once including asymmetric viewport placement") {
    std::vector<std::uint16_t> direct(RgbFrame::pixels), mirrored(RgbFrame::pixels);
    for (unsigned i = 0; i < RgbDiagnostics::names.size(); ++i) {
        const auto pattern = static_cast<RgbDiagnosticPattern>(i);
        CHECK(RgbDiagnostics::render(direct, pattern, {17, 33}, false));
        CHECK(RgbDiagnostics::render(mirrored, pattern, {17, 33}, true));
        for (int y = 0; y < 480; ++y)
            for (int x = 0; x < 800; ++x)
                CHECK_EQ(direct[y * 800 + x], mirrored[y * 800 + 799 - x]);
    }
}

TEST_CASE("invalid diagnostic patterns placements and buffers are rejected without writes") {
    std::vector<std::uint16_t> frame(RgbFrame::pixels, 0xabcd);
    CHECK(!RgbDiagnostics::render(frame, RgbDiagnosticPattern::Count, {}, false));
    CHECK(!RgbDiagnostics::render(frame, static_cast<RgbDiagnosticPattern>(255), {}, false));
    CHECK(!RgbDiagnostics::render(frame, RgbDiagnosticPattern::Viewport, {INT_MAX, INT_MIN}, false));
    CHECK(!RgbDiagnostics::render(std::span{frame}.first(1), RgbDiagnosticPattern::Red, {}, false));
    CHECK(std::all_of(frame.begin(), frame.end(), [](auto p) { return p == 0xabcd; }));
    CHECK(!RgbDiagnostics::parse("unknown"));
    CHECK_EQ(RgbDiagnostics::name(RgbDiagnosticPattern::Count), "invalid");
    auto pattern = RgbDiagnosticPattern::Red;
    for (const auto name : RgbDiagnostics::names) {
        CHECK_EQ(RgbDiagnostics::parse(name).value(), pattern);
        CHECK_EQ(RgbDiagnostics::previous(RgbDiagnostics::next(pattern)), pattern);
        pattern = RgbDiagnostics::next(pattern);
    }
    CHECK_EQ(pattern, RgbDiagnosticPattern::Red);
}

TEST_CASE("diagnostic sequence holds three seconds skips missed intervals and pauses without catchup") {
    RgbDiagnosticSequence sequence;
    CHECK(sequence.tick(100)); CHECK(!sequence.tick(3099));
    CHECK(sequence.tick(3100)); CHECK_EQ(sequence.pattern(), RgbDiagnosticPattern::Green);
    CHECK(sequence.tick(9100)); CHECK_EQ(sequence.pattern(), RgbDiagnosticPattern::White);
    sequence.set_automatic(false, 9100);
    CHECK(!sequence.tick(100000)); CHECK_EQ(sequence.pattern(), RgbDiagnosticPattern::White);
    sequence.select(RgbDiagnosticPattern::Viewport, 100000);
    sequence.set_automatic(true, 100000);
    CHECK(!sequence.tick(102999)); CHECK(sequence.tick(103000));
    CHECK_EQ(sequence.pattern(), RgbDiagnosticPattern::Red);
    CHECK(!sequence.tick(127000)); // Whole cycle leaves the same visible frame.
}

TEST_CASE("diagnostic sequence handles uint32 clock rollover") {
    RgbDiagnosticSequence sequence;
    CHECK(sequence.tick(std::numeric_limits<std::uint32_t>::max() - 1000));
    CHECK(!sequence.tick(1998)); CHECK(sequence.tick(1999));
    CHECK_EQ(sequence.pattern(), RgbDiagnosticPattern::Green);
}
}  // namespace
