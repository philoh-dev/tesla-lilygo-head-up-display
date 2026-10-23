#include "test.hpp"
#include "lvgl_sdl_display.hpp"

#include <climits>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <vector>

namespace {
using tesla_headup::simulator::LvglSdlDisplay;
using tesla_headup::simulator::PreviewOptions;
using tesla_headup::platform::RgbFrame;

PreviewOptions parse(std::initializer_list<const char*> arguments) {
    return PreviewOptions::parse(static_cast<int>(arguments.size()), arguments.begin());
}

class Fixture {
public:
    explicit Fixture(PreviewOptions options = {}, bool expect_ready = true) : options_(options) {
        SDL_SetMainReady();
        CHECK_EQ(SDL_Init(SDL_INIT_VIDEO), 0);
        surface_ = SDL_CreateRGBSurfaceWithFormat(0, options.output_width(),
            options.output_height(), 16, SDL_PIXELFORMAT_RGB565);
        CHECK(surface_);
        renderer_ = SDL_CreateSoftwareRenderer(surface_);
        CHECK(renderer_);
        display = std::make_unique<LvglSdlDisplay>(*renderer_, options);
        CHECK_EQ(display->ready(), expect_ready);
        lv_tick_set_cb([]() -> std::uint32_t { return 0; });
        display->set_projection_mirrored(false);
    }
    ~Fixture() {
        display.reset();
        SDL_DestroyRenderer(renderer_);
        SDL_FreeSurface(surface_);
        SDL_Quit();
    }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    [[nodiscard]] std::vector<std::uint16_t> pixels() const {
        std::vector<std::uint16_t> frame(
            static_cast<std::size_t>(options_.output_width()) * options_.output_height());
        CHECK_EQ(SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGB565,
            frame.data(), options_.output_width() * 2), 0);
        return frame;
    }
    std::unique_ptr<LvglSdlDisplay> display;

private:
    PreviewOptions options_;
    SDL_Surface* surface_{nullptr};
    SDL_Renderer* renderer_{nullptr};
};

void draw(LvglSdlDisplay& display, std::string_view speed = "135") {
    using namespace tesla_headup::application;
    auto& canvas = display.canvas();
    canvas.begin_frame();
    canvas.draw_text({FeatureId::VehicleSpeed, speed, TextRole::PrimaryValue});
    canvas.draw_text({FeatureId::VehicleSpeed, "km/h", TextRole::Unit});
    canvas.draw_text({FeatureId::SelectedGear, "D", TextRole::Gear});
    canvas.draw_symbol({FeatureId::TurnSignals, Symbol::LeftArrow, SymbolRole::TurnSignal,
        Anchor::Center, -175, -58, 32, 30, true, true});
    canvas.draw_symbol({FeatureId::Autopilot, Symbol::Autopilot, SymbolRole::Autopilot,
        Anchor::Center, -175, 60, 32, 30, true, true});
    canvas.end_frame();
    display.refresh();
}

std::vector<std::uint16_t> logical_frame(std::string_view speed = "135") {
    Fixture fixture;
    draw(*fixture.display, speed);
    return fixture.pixels();
}

TEST_CASE("simulator defaults stay LilyGO and RGB placement is order independent") {
    auto options = parse({"simulator"});
    CHECK(options.error.empty());
    CHECK(!options.rgb);
    CHECK_EQ(options.output_width(), 536);
    CHECK_EQ(options.output_height(), 240);
    options = parse({"simulator", "--rgb-viewport"});
    CHECK(options.rgb);
    CHECK_EQ(options.output_width(), 800);
    CHECK_EQ(options.output_height(), 480);
    CHECK_EQ(options.origin.x, 132);
    CHECK_EQ(options.origin.y, 0);
    options = parse({"simulator", "--origin-x", "17", "--origin-y", "33", "--rgb-viewport"});
    CHECK(options.error.empty());
    CHECK_EQ(options.origin.x, 17);
    CHECK_EQ(options.origin.y, 33);
    options = parse({"simulator", "--rgb-viewport", "--origin-x", "264", "--origin-y", "240"});
    CHECK(options.error.empty());
    options = parse({"simulator", "--screenshot", "--rgb"});
    CHECK(!options.rgb); // Do not interpret a filename as an option.
}

TEST_CASE("simulator rejects missing malformed overflowing and non-fitting RGB origins") {
    for (const char* value : {"", "abc", "1.5", "1x", "2147483648", "-2147483649",
                              "-1", "265"}) {
        CHECK(!parse({"simulator", "--rgb", "--origin-x", value}).error.empty());
    }
    CHECK(!parse({"simulator", "--rgb", "--origin-y", "241"}).error.empty());
    CHECK(!parse({"simulator", "--rgb", "--origin-y"}).error.empty());
    CHECK(!parse({"simulator", "--origin-x", "0"}).error.empty());
    CHECK(!parse({"simulator", "--origin-y", "0"}).error.empty());
}

TEST_CASE("simulator preview speeds preserve defaults and reject malformed or out of range input") {
    CHECK_EQ(parse({"simulator"}).preview_speed, 135);
    CHECK_EQ(parse({"simulator"}).preview_speed_limit, 50);
    const auto options = parse({"simulator", "--preview-speed", "130", "--preview-speed-limit", "130"});
    CHECK(options.error.empty());
    CHECK_EQ(options.preview_speed, 130);
    CHECK_EQ(options.preview_speed_limit, 130);
    CHECK(parse({"simulator", "--preview-speed", "0"}).error.empty());
    CHECK(parse({"simulator", "--preview-speed", "400"}).error.empty());
    for (const auto flag : {"--preview-speed", "--preview-speed-limit"}) {
        CHECK(!parse({"simulator", flag}).error.empty());
        for (const auto value : {"-1", "abc", "1.5", "1x", "9999999999999999999"})
            CHECK(!parse({"simulator", flag, value}).error.empty());
    }
    CHECK(!parse({"simulator", "--preview-speed", "401"}).error.empty());
    CHECK(!parse({"simulator", "--preview-speed-limit", "0"}).error.empty());
    CHECK(!parse({"simulator", "--preview-speed-limit", "151"}).error.empty());
}

TEST_CASE("SDL RGB presentation matches the firmware composer and legacy renderer colors") {
    const auto logical = logical_frame();
    Fixture fixture{PreviewOptions{.rgb = true, .viewport = true}};
    draw(*fixture.display);
    std::vector<std::uint16_t> expected(RgbFrame::pixels);
    for (const auto origin : {tesla_headup::platform::RgbPlacement{132, 0}, {17, 33},
                              {0, 0}, {264, 240}, {132, 120}}) {
        CHECK(fixture.display->set_dashboard_position(origin.x, origin.y));
        for (const bool mirror : {false, true}) {
            fixture.display->set_projection_mirrored(mirror);
            CHECK(RgbFrame::compose(logical, expected, origin, mirror));
            CHECK(fixture.pixels() == expected);
        }
    }
    const auto previous = fixture.pixels();
    CHECK(!fixture.display->set_dashboard_position(-1, 0));
    CHECK(!fixture.display->set_dashboard_position(0, 241));
    CHECK(fixture.pixels() == previous);
}

TEST_CASE("RGB viewport movement clamps at every edge without changing the logical dashboard") {
    const auto logical = logical_frame();
    Fixture fixture{PreviewOptions{.rgb = true, .viewport = true}};
    draw(*fixture.display);
    fixture.display->move_dashboard(-1, 10);
    CHECK_EQ(fixture.display->dashboard_position().x, 131);
    CHECK_EQ(fixture.display->dashboard_position().y, 10);
    fixture.display->move_dashboard(INT_MIN, INT_MIN);
    CHECK_EQ(fixture.display->dashboard_position().x, 0);
    CHECK_EQ(fixture.display->dashboard_position().y, 0);
    fixture.display->move_dashboard(INT_MAX, INT_MAX);
    CHECK_EQ(fixture.display->dashboard_position().x, 264);
    CHECK_EQ(fixture.display->dashboard_position().y, 240);
    std::vector<std::uint16_t> expected(RgbFrame::pixels);
    CHECK(RgbFrame::compose(logical, expected, {264, 240}, false));
    CHECK(fixture.pixels() == expected);
}

TEST_CASE("native RGB partial redraw clears old glyphs after relocation and mirroring") {
    const auto logical = logical_frame("1");
    Fixture fixture{PreviewOptions{.rgb = true, .viewport = true}};
    draw(*fixture.display);
    CHECK(fixture.display->set_dashboard_position(0, 240));
    fixture.display->set_projection_mirrored(true);
    draw(*fixture.display, "1");
    std::vector<std::uint16_t> expected(RgbFrame::pixels);
    CHECK(RgbFrame::compose(logical, expected, {0, 240}, true));
    CHECK(fixture.pixels() == expected);
}

TEST_CASE("legacy SDL mirror and placement behavior remain unchanged") {
    Fixture fixture;
    draw(*fixture.display);
    const auto direct = fixture.pixels();
    CHECK(!fixture.display->set_dashboard_position(132, 120));
    fixture.display->move_dashboard(INT_MAX, INT_MAX);
    CHECK(fixture.pixels() == direct);
    fixture.display->set_projection_mirrored(true);
    const auto mirrored = fixture.pixels();
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 536; ++x) {
            CHECK_EQ(direct[y * 536 + x], mirrored[y * 536 + 535 - x]);
        }
    }
}

TEST_CASE("SDL screenshot exports native RGB dimensions and pixels") {
    Fixture fixture{PreviewOptions{.rgb = true, .viewport = true}};
    draw(*fixture.display);
    const auto path = std::filesystem::current_path() / "sdl-rgb-export-test.bmp";
    CHECK(!std::filesystem::exists(path));
    CHECK(fixture.display->save_screenshot(path.string().c_str()));
    SDL_Surface* loaded = SDL_LoadBMP(path.string().c_str());
    std::filesystem::remove(path);
    CHECK(loaded);
    CHECK_EQ(loaded->w, 800);
    CHECK_EQ(loaded->h, 480);
    SDL_Surface* native = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGB565, 0);
    SDL_FreeSurface(loaded);
    CHECK(native);
    const auto expected = fixture.pixels();
    bool equal = true;
    for (int y = 0; y < 480; ++y) {
        const auto* row = reinterpret_cast<const std::uint16_t*>(
            static_cast<const std::uint8_t*>(native->pixels) + y * native->pitch);
        equal = equal && std::equal(row, row + 800, expected.begin() + y * 800);
    }
    SDL_FreeSurface(native);
    CHECK(equal);
}

TEST_CASE("SDL diagnostics use the firmware generator for every pattern without a dashboard") {
    const auto options = parse({"simulator", "--rgb-diagnostics", "--diagnostic-pattern", "viewport"});
    CHECK(options.error.empty()); CHECK(options.rgb); CHECK(options.diagnostics);
    CHECK(options.explicit_pattern);
    CHECK(!parse({"simulator", "--diagnostic-pattern", "red"}).error.empty());
    CHECK(!parse({"simulator", "--rgb-diagnostics", "--diagnostic-pattern"}).error.empty());
    CHECK(!parse({"simulator", "--rgb-diagnostics", "--diagnostic-pattern", "unknown"}).error.empty());
    Fixture fixture{options};
    CHECK(!fixture.display->canvas().ready());
    CHECK(fixture.display->set_dashboard_position(17, 33));
    std::vector<std::uint16_t> expected(RgbFrame::pixels);
    using tesla_headup::platform::RgbDiagnostics;
    using tesla_headup::platform::RgbDiagnosticPattern;
    for (unsigned i = 0; i < RgbDiagnostics::names.size(); ++i) {
        const auto pattern = static_cast<RgbDiagnosticPattern>(i);
        CHECK(fixture.display->set_diagnostic_pattern(pattern));
        for (const bool mirror : {false, true}) {
            fixture.display->set_projection_mirrored(mirror);
            CHECK(RgbDiagnostics::render(expected, pattern, {17, 33}, mirror));
            CHECK(fixture.pixels() == expected);
        }
    }
    const auto old = fixture.pixels();
    CHECK(!fixture.display->set_diagnostic_pattern(RgbDiagnosticPattern::Count));
    CHECK(fixture.pixels() == old);
}

TEST_CASE("SDL rejects diagnostic options without RGB output even for direct API callers") {
    Fixture fixture{PreviewOptions{.diagnostics = true}, false};
    CHECK(!fixture.display->ready());
    CHECK(!fixture.display->canvas().ready());
}

#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
TEST_CASE("RGB road scene defaults to native full-panel rendering and rejects nonzero placement") {
    const auto options = parse({"simulator", "--rgb"});
    CHECK(options.error.empty()); CHECK(options.full_road_scene());
    CHECK_EQ(options.origin.x, 0); CHECK_EQ(options.origin.y, 0);
    CHECK_EQ(options.surface().pixels(), RgbFrame::pixels);
    CHECK(!parse({"simulator", "--rgb", "--origin-x", "1"}).error.empty());
    Fixture fixture{options};
    draw(*fixture.display);
    const auto direct = fixture.pixels();
    fixture.display->set_projection_mirrored(true);
    const auto mirrored = fixture.pixels();
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 800; ++x)
            CHECK_EQ(direct[y * 800 + x], mirrored[y * 800 + 799 - x]);
    CHECK(!fixture.display->set_dashboard_position(1, 0));
    fixture.display->move_dashboard(INT_MAX, INT_MIN);
    CHECK_EQ(fixture.display->dashboard_position().x, 0);
    CHECK_EQ(fixture.display->dashboard_position().y, 0);
}
#endif
}  // namespace
