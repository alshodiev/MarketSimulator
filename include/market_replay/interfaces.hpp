#ifndef MARKET_REPLAY_INTERFACES_HPP
#define MARKET_REPLAY_INTERFACES_HPP

#include "event.hpp" // For OrderRequest

namespace market_replay {

// Interface for strategies to submit orders to the simulation core
class IOrderSubmitter {
public:
    virtual ~IOrderSubmitter() = default;
    virtual void submit_order_request(OrderRequest request) = 0; 
    // virtual void cancel_order_request(StrategyId strategy_id, OrderId client_order_id, const std::string& symbol, Timestamp request_timestamp) = 0;
    // Note: submit_order_request takes by value to allow modification (e.g. setting strategy_id if not already there)
    // or if the callee needs to own it. Pass by const& if modification is not needed and lifetime is managed.
    // For queueing, by value (move semantics) is often good.
};

} // namespace market_replay
#endif // MARKET_REPLAY_INTERFACES_HPP