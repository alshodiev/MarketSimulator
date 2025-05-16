#ifndef MARKET_REPLAY_EVENT_HPP
#define MARKET_REPLAY_EVENT_HPP

#include "common.hpp"
#include <string>
#include <variant>
#include <memory>

namespace market_replay {

class IOrderSubmitter; 

enum class EventType : uint8_t {
    UNKNOWN,
    QUOTE,
    TRADE,
    ORDER_ACK,
    SIM_CONTROL_DISPATCHER, // For dispatcher internal control (e.g., process order requests)
    SIM_CONTROL_STRATEGY    // For signaling strategies (e.g., shutdown)
};

struct BaseEvent {
    Timestamp exchange_timestamp; // Timestamp from the data feed (when it occurred at exchange)
    Timestamp arrival_timestamp;  // Timestamp when event effectively arrives (at strategy or internal component)
    EventType type = EventType::UNKNOWN;

    explicit BaseEvent(Timestamp ex_ts, EventType et = EventType::UNKNOWN)
        : exchange_timestamp(ex_ts), arrival_timestamp(ex_ts), type(et) {}
    virtual ~BaseEvent() = default;

    // For priority queue sorting. This is the time the event should be processed by the recipient.
    virtual Timestamp get_effective_timestamp() const { return arrival_timestamp; }
};

struct EventComparator {
    bool operator()(const std::unique_ptr<BaseEvent>& a, const std::unique_ptr<BaseEvent>& b) const {
        if (!a || !b) return b == nullptr; // Handle nullptrs, useful for sentinel
        return a->get_effective_timestamp() > b->get_effective_timestamp(); // Min-heap
    }
};

struct QuoteEvent : public BaseEvent {
    std::string symbol;
    Price bid_price;
    Quantity bid_size;
    Price ask_price;
    Quantity ask_size;

    QuoteEvent(Timestamp ex_ts, std::string sym, Price bp, Quantity bs, Price ap, Quantity as)
        : BaseEvent(ex_ts, EventType::QUOTE), symbol(std::move(sym)),
          bid_price(bp), bid_size(bs), ask_price(ap), ask_size(as) {}
};

struct TradeEvent : public BaseEvent {
    std::string symbol;
    Price price;
    Quantity size;
    // Could add aggressor side if available in data

    TradeEvent(Timestamp ex_ts, std::string sym, Price p, Quantity s)
        : BaseEvent(ex_ts, EventType::TRADE), symbol(std::move(sym)), price(p), size(s) {}
};

// This struct is used by strategies to request an order, not an event for the MEPQ directly
// but for the OrderRequestQueue.
struct OrderRequest {
    StrategyId strategy_id;
    OrderId client_order_id; 
    std::string symbol;
    OrderSide side;
    OrderType type;
    Price price;      // Limit price for LIMIT orders, 0 or NaN for MARKET
    Quantity quantity;
    Timestamp request_timestamp; // When strategy logic decided to send this (arrival_timestamp of causal event + processing)
    // Optional: TimeInForce, etc.
};

struct OrderAckEvent : public BaseEvent {
    StrategyId strategy_id; // To route ack to correct strategy
    OrderId client_order_id;
    OrderId exchange_order_id; // Assigned by simulated exchange
    std::string symbol;
    OrderStatus status;
    Price last_filled_price = 0.0;
    Quantity last_filled_quantity = 0;
    Quantity cumulative_filled_quantity = 0;
    Quantity leaves_quantity = 0;
    std::string reject_reason;

    OrderAckEvent(Timestamp effective_ts, StrategyId strat_id, OrderId cl_ord_id, OrderId ex_ord_id, std::string sym, OrderStatus st)
        : BaseEvent(effective_ts, EventType::ORDER_ACK), // effective_ts is when this ack arrives at strategy
          strategy_id(std::move(strat_id)), client_order_id(cl_ord_id),
          exchange_order_id(ex_ord_id), symbol(std::move(sym)), status(st) {
        this->arrival_timestamp = effective_ts; // Explicitly set for clarity
    }
};

// For signaling within Dispatcher or to Strategies
struct SimControlEvent : public BaseEvent {
    enum class ControlType {
        END_OF_DATA_FEED, // From CSV parser to Dispatcher
        PROCESS_ORDER_REQUESTS, // Dispatcher internal periodic task
        STRATEGY_SHUTDOWN // Dispatcher to Strategy
    };
    ControlType control_type;
    StrategyId target_strategy_id; // Optional: if event is for a specific strategy

    SimControlEvent(Timestamp effective_ts, ControlType ct, EventType evt_type = EventType::SIM_CONTROL_DISPATCHER) 
        : BaseEvent(effective_ts, evt_type), control_type(ct) {
        this->arrival_timestamp = effective_ts;
    }
};


// For Strategy input queues
using StrategyInputEventVariant = std::variant<
    std::unique_ptr<QuoteEvent>,
    std::unique_ptr<TradeEvent>,
    std::unique_ptr<OrderAckEvent>,
    std::unique_ptr<SimControlEvent> // For shutdown signal
>;


} // namespace market_replay

#endif // MARKET_REPLAY_EVENT_HPP