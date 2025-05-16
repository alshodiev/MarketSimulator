#ifndef MARKET_REPLAY_TEST_UTILS_HPP
#define MARKET_REPLAY_TEST_UTILS_HPP

#include "market_replay/common.hpp" // For market_replay::Timestamp and timestamp_to_string
#include "catch2/catch_tostring.hpp" // Need this for StringMaker base
#include <string>
#include <ostream>

// --- Custom StringMaker for market_replay::Timestamp ---
namespace Catch {
    template<>
    struct StringMaker<market_replay::Timestamp> {
        static std::string convert(market_replay::Timestamp const& ts) {
            return market_replay::timestamp_to_string(ts) + "ns (custom)"; 
        }
    };
}
// --- End Custom StringMaker ---

#endif //MARKET_REPLAY_TEST_UTILS_HPP