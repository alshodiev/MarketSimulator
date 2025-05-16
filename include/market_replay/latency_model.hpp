#ifndef MARKET_REPLAY_LATENCY_MODEL_HPP
#define MARKET_REPLAY_LATENCY_MODEL_HPP

#include "common.hpp"
#include "event.hpp"
#include <random> // For potential future use with distributions

namespace market_replay {

class LatencyModel {
public:
    // Configurable latencies
    struct Config {
        Duration market_data_feed_latency = std::chrono::microseconds(50);  // Exchange source to strategy input queue
        Duration strategy_processing_latency = std::chrono::microseconds(5); // Time strategy "thinks"
        Duration order_network_latency_strat_to_exch = std::chrono::microseconds(20); // Strategy output to exchange input
        Duration exchange_order_processing_latency = std::chrono::microseconds(10); // Exchange internal for ack
        Duration exchange_fill_processing_latency = std::chrono::microseconds(15);  // Exchange internal for fill (can be > ack)
        Duration ack_network_latency_exch_to_strat = std::chrono::microseconds(20);   // Exchange output to strategy input (for ack/fill)
    };

    explicit LatencyModel(Config config); // Constructor that takes a Config
    LatencyModel();                      // Default constructor (will use Config's defaults)

    // Latency for market data (quote/trade) from exchange to strategy's input queue
    Duration get_market_data_latency(const BaseEvent& event) const;

    // Timestamp when an order (sent at `strategy_decision_ts`) arrives at the exchange
    Timestamp get_order_arrival_at_exchange_ts(Timestamp strategy_decision_ts) const;
    
    // Timestamp when a simple ACK for an order (that arrived at `order_arrival_at_exchange_ts`)
    // arrives back at the strategy's input queue.
    Timestamp get_ack_arrival_at_strategy_ts(Timestamp order_arrival_at_exchange_ts) const;

    // Timestamp when a FILL for an order (that arrived at `order_arrival_at_exchange_ts`)
    // arrives back at the strategy's input queue.
    Timestamp get_fill_arrival_at_strategy_ts(Timestamp order_arrival_at_exchange_ts) const;
    
    // The time a strategy is assumed to take to process an event and decide on an action
    Duration get_strategy_processing_latency() const;


private:
    Config config_;
    // std::mt19937 random_generator_; // For distributions if needed
};

} // namespace market_replay
#endif // MARKET_REPLAY_LATENCY_MODEL_HPP