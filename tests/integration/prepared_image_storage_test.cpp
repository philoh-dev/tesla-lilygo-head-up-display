#include "test.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/prepared_image_storage.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/indicator_images.hpp"
#include <array>

namespace {
using namespace tesla_headup::platform::assets;

TEST_CASE("prepared image unpacking accepts literals and overlapping LZ4 matches") {
    constexpr std::array<std::uint8_t, 9> packed{0x13, 'a', 1, 0, 0x40, 'b', 'c', 'd', 'e'};
    std::array<std::uint8_t, 12> output{};
    CHECK(unpack_image(packed, output));
    constexpr std::array<std::uint8_t, 12> expected{'a','a','a','a','a','a','a','a','b','c','d','e'};
    CHECK(output == expected);
    for (std::size_t length = 0; length < packed.size(); ++length)
        CHECK(!unpack_image(std::span{packed}.first(length), output));
    std::array<std::uint8_t, 11> small{};
    CHECK(!unpack_image(packed, small));
    constexpr std::array<std::uint8_t, 3> zero_offset{0, 0, 0};
    CHECK(!unpack_image(zero_offset, output));
    constexpr std::array<std::uint8_t, 3> past_start{0, 1, 0};
    CHECK(!unpack_image(past_start, output));
    constexpr std::array<std::uint8_t, 2> unterminated_length{0xF0, 255};
    CHECK(!unpack_image(unterminated_length, output));
}

TEST_CASE("prepared image decoder keeps arbitrary malformed blocks within output bounds") {
    std::uint32_t random = 12345;
    std::array<std::uint8_t, 64> input{};
    std::array<std::uint8_t, 96> guarded{};
    for (unsigned trial = 0; trial < 5000; ++trial) {
        for (auto& byte : input) {
            random = random * 1664525U + 1013904223U;
            byte = static_cast<std::uint8_t>(random >> 24U);
        }
        guarded.fill(0xA5);
        (void)unpack_image(std::span{input}.first(trial % 65), std::span{guarded}.subspan(8, 80));
        for (std::size_t index = 0; index < 8; ++index) {
            CHECK_EQ(guarded[index], 0xA5);
            CHECK_EQ(guarded[88 + index], 0xA5);
        }
    }
}

TEST_CASE("compact Park assets share bounded storage independently of native assets") {
    CHECK(retain_road_scene_compact_images());
    const auto* data = road_scene_park_image.data;
    CHECK(data != nullptr);
    CHECK_EQ(road_scene_park_image.data_size, 256U * 160U * 3U);
    CHECK(retain_road_scene_compact_images());
    CHECK(retain_road_scene_wide_images());
    release_road_scene_compact_images();
    CHECK_EQ(road_scene_park_image.data, data);
    release_road_scene_wide_images();
    CHECK_EQ(road_scene_park_image.data, data);
    release_road_scene_compact_images();
    CHECK_EQ(road_scene_park_image.data, nullptr);
    CHECK(retain_road_scene_compact_images());
    release_road_scene_compact_images();
}

TEST_CASE("native assets initialize once share storage and release only the final owner") {
    CHECK(retain_road_scene_wide_images());
    const auto* data = road_scene_park_wide_image.data;
    CHECK(data != nullptr);
    CHECK(retain_road_scene_wide_images());
    CHECK_EQ(road_scene_park_wide_image.data, data);
    release_road_scene_wide_images();
    CHECK_EQ(road_scene_park_wide_image.data, data);
    release_road_scene_wide_images();
    CHECK_EQ(road_scene_park_wide_image.data, nullptr);
    CHECK_EQ(autopilot_road_scene_wide_image.data, nullptr);
    CHECK(retain_road_scene_wide_images());
    release_road_scene_wide_images();
    lv_image_dsc_t invalid{};
    const std::array<std::uint8_t, 3> bad{0, 0, 0};
    std::array images{PreparedImageStorage{&invalid, bad, 12}};
    unsigned users = 0;
    CHECK(!retain_images(images, users));
    CHECK_EQ(users, 0U);
    CHECK_EQ(invalid.data, nullptr);
}
}  // namespace
