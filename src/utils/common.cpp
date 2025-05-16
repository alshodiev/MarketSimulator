#include "market_replay/common.hpp"
#include <stdexcept> // For std::invalid_argument, std::stoll
#include <algorithm> // For std::transform
#include <cctype>    // For std::tolower

namespace market_replay {

Timestamp string_to_timestamp(const std::string& ts_str) {
    try {
        return Timestamp(std::chrono::nanoseconds(std::stoll(ts_str)));
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid timestamp string: " + ts_str + " - " + e.what());
    }
}

std::string timestamp_to_string(Timestamp ts) {
    return std::to_string(ts.time_since_epoch().count());
}

Duration string_to_duration_ns(const std::string& dur_str) {
    if (dur_str.empty()) {
        return Duration(0);
    }

    std::string lower_dur_str = dur_str;
    std::transform(lower_dur_str.begin(), lower_dur_str.end(), lower_dur_str.begin(), ::tolower);

    long long value;
    std::size_t unit_pos;

    try {
        value = std::stoll(lower_dur_str, &unit_pos);
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid duration string (value part): " + dur_str);
    }
    
    std::string unit = lower_dur_str.substr(unit_pos);

    if (unit == "ns") {
        return std::chrono::nanoseconds(value);
    } else if (unit == "us" || unit == "micros") {
        return std::chrono::microseconds(value);
    } else if (unit == "ms" || unit == "millis") {
        return std::chrono::milliseconds(value);
    } else if (unit == "s" || unit == "sec") {
        return std::chrono::seconds(value);
    } else if (unit.empty() && value == 0) { // "0" implies 0ns
         return std::chrono::nanoseconds(0);
    } else {
        throw std::invalid_argument("Invalid or unsupported duration unit in: " + dur_str);
    }
}

} // namespace market_replay