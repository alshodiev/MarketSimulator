#include "catch2/catch_test_macros.hpp"
#include "market_replay/common.hpp"

TEST_CASE("Timestamp Conversions", "[common]") {
    SECTION("String to Timestamp and back") {
        std::string ts_str = "1678886400000000000";
        market_replay::Timestamp ts = market_replay::string_to_timestamp(ts_str);
        REQUIRE(market_replay::timestamp_to_string(ts) == ts_str);
    }
    SECTION("Invalid timestamp string") {
        REQUIRE_THROWS_AS(market_replay::string_to_timestamp("not_a_number"), std::invalid_argument);
    }
}

TEST_CASE("Duration Conversions", "[common]") {
    using namespace std::chrono_literals;
    SECTION("Nanoseconds") {
        REQUIRE(market_replay::string_to_duration_ns("100ns") == 100ns);
    }
    SECTION("Microseconds") {
        REQUIRE(market_replay::string_to_duration_ns("50us") == 50us);
        REQUIRE(market_replay::string_to_duration_ns("50micros") == 50us);
    }
    SECTION("Milliseconds") {
        REQUIRE(market_replay::string_to_duration_ns("20ms") == 20ms);
        REQUIRE(market_replay::string_to_duration_ns("20millis") == 20ms);
    }
    SECTION("Seconds") {
        REQUIRE(market_replay::string_to_duration_ns("2s") == 2s);
         REQUIRE(market_replay::string_to_duration_ns("2sec") == 2s);
    }
    SECTION("Zero") {
        REQUIRE(market_replay::string_to_duration_ns("0") == 0ns);
         REQUIRE(market_replay::string_to_duration_ns("0ns") == 0ns);
    }
    SECTION("Invalid duration string") {
        REQUIRE_THROWS_AS(market_replay::string_to_duration_ns("not_a_duration"), std::invalid_argument);
        REQUIRE_THROWS_AS(market_replay::string_to_duration_ns("100xyz"), std::invalid_argument);
        REQUIRE_THROWS_AS(market_replay::string_to_duration_ns("ms"), std::invalid_argument); // Number missing
    }
}