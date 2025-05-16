#include "market_replay/latency_model.hpp"
#include "market_replay/event.hpp" // For BaseEvent if type-specific latency needed

namespace market_replay {

LatencyModel::LatencyModel(Config config) : config_(config) {
    // If using random distributions, initialize here, e.g., with a fixed seed for determinism
    // random_generator_.seed(std::random_device{}()); // Or a fixed seed
}

LatencyModel::LatencyModel() : config_({}) { // or just config_() for C++11 onwards
}

Duration LatencyModel::get_market_data_latency(const BaseEvent& /*event*/) const {
    // Could make this event-dependent (e.g. different latency for different symbols/exchanges)
    return config_.market_data_feed_latency;
}

Duration LatencyModel::get_strategy_processing_latency() const {
    return config_.strategy_processing_latency;
}

Timestamp LatencyModel::get_order_arrival_at_exchange_ts(Timestamp strategy_decision_ts) const {
    // strategy_decision_ts is when the strategy finished its logic and called submit_order.
    // This includes its internal processing.
    return strategy_decision_ts + config_.order_network_latency_strat_to_exch;
}

Timestamp LatencyModel::get_ack_arrival_at_strategy_ts(Timestamp order_arrival_at_exchange_ts) const {
    Timestamp ack_leaves_exchange_ts = order_arrival_at_exchange_ts + config_.exchange_order_processing_latency;
    return ack_leaves_exchange_ts + config_.ack_network_latency_exch_to_strat;
}

Timestamp LatencyModel::get_fill_arrival_at_strategy_ts(Timestamp order_arrival_at_exchange_ts) const {
    // Assumes fill processing starts after order arrival.
    // If fill processing is separate from ack, it could be:
    // fill_leaves_exchange_ts = order_arrival_at_exchange_ts + config_.exchange_fill_processing_latency
    // return fill_leaves_exchange_ts + config_.ack_network_latency_exch_to_strat;
    // Or, if a fill implies an ack path:
    Timestamp processing_done_ts = order_arrival_at_exchange_ts + config_.exchange_fill_processing_latency;
    return processing_done_ts + config_.ack_network_latency_exch_to_strat;
}


} // namespace market_replay