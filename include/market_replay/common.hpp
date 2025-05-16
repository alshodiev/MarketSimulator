#ifndef MARKET_REPLAY_COMMON_HPP
#define MARKET_REPLAY_COMMON_HPP

#include <chrono>
#include <string>
#include <cstdint>
#include <variant>
#include <optional>
#include <limits> // For std::numeric_limits

namespace market_replay {

// Using nanoseconds for high precision timestamps
using Timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
using Duration = std::chrono::nanoseconds;

constexpr double PRICE_EPSILON = 1e-9; // For double comparisons
using Price = double;
constexpr Price INVALID_PRICE = std::numeric_limits<Price>::quiet_NaN();


// Helper to convert string to Timestamp (assuming specific format like epoch ns)
Timestamp string_to_timestamp(const std::string& ts_str);
std::string timestamp_to_string(Timestamp ts);
Duration string_to_duration_ns(const std::string& dur_str); // e.g., "100ns", "50us", "10ms"

enum class OrderSide : uint8_t {
    BUY,
    SELL
};

enum class OrderType : uint8_t {
    MARKET,
    LIMIT
};

enum class OrderStatus : uint8_t {
    PENDING_NEW,    // Strategy submitted, not yet processed by simulator's order manager
    NEW,            // Simulator's order manager accepted, sent to "exchange"
    ACKNOWLEDGED,   // "Exchange" acknowledged (e.g., for a passive limit order)
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED,
    REJECTED,
    EXPIRED         // If Time-In-Force is implemented
};

using Quantity = uint64_t;
using OrderId = uint64_t; 
using StrategyId = std::string;


struct PnL {
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0; 
    double total_volume_traded = 0.0;
    long long current_position = 0; // Signed: + for long, - for short
};

} // namespace market_replay

#endif // MARKET_REPLAY_COMMON_HPP