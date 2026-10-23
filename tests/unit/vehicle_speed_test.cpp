#include "test.hpp"

#include "tesla_headup/domain/vehicle_speed.hpp"

using tesla_headup::domain::VehicleSpeed;

TEST_CASE("vehicle speed accepts the supported metric range") {
    const auto stopped = VehicleSpeed::from_kilometres_per_hour(0);
    const auto maximum = VehicleSpeed::from_kilometres_per_hour(400);

    CHECK(stopped.has_value());
    CHECK(maximum.has_value());
    CHECK_EQ(stopped->kilometres_per_hour(), 0);
    CHECK_EQ(maximum->kilometres_per_hour(), 400);
}

TEST_CASE("vehicle speed rejects values outside the supported range") {
    CHECK(!VehicleSpeed::from_kilometres_per_hour(401).has_value());
}
