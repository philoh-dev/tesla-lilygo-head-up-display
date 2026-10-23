#include "test.hpp"
#include "board.hpp"

namespace {
namespace board = tesla_headup::board::devkitc_n32r16v;

TEST_CASE("DevKitC RGB pins are unique and do not consume reserved interfaces") {
    CHECK(board::valid_display_pin_assignment());
    CHECK_EQ(board::rgb_data_pins.size(), 16U);
    CHECK_EQ(board::display_pins.size(), 22U);
    for (const int pin : board::display_pins) {
        CHECK(board::is_safe_display_pin(pin));
        CHECK(pin != 0 && pin != 3 && pin != 45 && pin != 46);
        CHECK(pin != 19 && pin != 20 && pin != 43 && pin != 44);
        CHECK(pin < 26 || pin > 38);
        CHECK(pin != 48);
    }
}

TEST_CASE("DevKitC safe pin filter rejects memory strapping USB and LED pins") {
    for (const int pin : {-1, 0, 3, 19, 20, 22, 25, 26, 33, 34, 35, 36, 37,
                          38, 43, 44, 45, 46, 48, 49}) {
        CHECK(!board::is_safe_display_pin(pin));
    }
}

TEST_CASE("DevKitC profile reserves RGB565 bits in ESP-IDF order") {
    CHECK_EQ(board::rgb_data_pins[0], 4);   // B3
    CHECK_EQ(board::rgb_data_pins[4], 8);   // B7
    CHECK_EQ(board::rgb_data_pins[5], 9);   // G2
    CHECK_EQ(board::rgb_data_pins[10], 14); // G7
    CHECK_EQ(board::rgb_data_pins[11], 15); // R3
    CHECK_EQ(board::rgb_data_pins[15], 21); // R7
    CHECK_EQ(board::flash_bytes, 33'554'432U);
    CHECK_EQ(board::psram_bytes, 16'777'216U);
}
}  // namespace
