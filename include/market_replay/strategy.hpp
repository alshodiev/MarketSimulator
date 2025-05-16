#ifndef MARKET_REPLAY_STRATEGY_HPP
#define MARKET_REPLAY_STRATEGY_HPP

#include "event.hpp"
#include "interfaces.hpp"
#include "logger.hpp" // For logging within strategy
#include "metrics.hpp"
#include <string>
#include <memory> // For std::shared_ptr

namespace market_replay {

class IStrategy {
public:
    friend struct StrategyEventVisitor;
    IStrategy(StrategyId id, 
              IOrderSubmitter* order_submitter, 
              std::shared_ptr<MetricsCollector> metrics_collector)
        : id_(std::move(id)), 
          order_submitter_(order_submitter),
          metrics_collector_(metrics_collector),
          next_client_order_id_(1)
         {}
    virtual ~IStrategy() = default;

    // Called once when the strategy thread starts, before event loop
    virtual void on_init(Timestamp current_sim_time) {} 
    // Called for each event from the strategy's input queue
    virtual void on_event(const StrategyInputEventVariant& event_variant, Timestamp strategy_arrival_ts) = 0;
    // Called once when the strategy thread is about to shut down
    virtual void on_shutdown(Timestamp current_sim_time) {}

    const StrategyId& get_id() const { return id_; }

protected:
    OrderId get_next_client_order_id() { return next_client_order_id_++; }
    
    // Helper to submit order
    void submit_order(const std::string& symbol, OrderSide side, OrderType type, Price price, Quantity quantity, Timestamp decision_ts) {
        if (!order_submitter_) {
            LOG_WARN("Strategy {}: Order submitter not set, cannot submit order.", id_);
            return;
        }
        OrderRequest req{
            id_,
            get_next_client_order_id(),
            symbol,
            side,
            type,
            price,
            quantity,
            decision_ts // This is critical: when the strategy made the decision.
        };
        LOG_DEBUG("Strategy {}: Submitting order ClientID={}, Symbol={}, Side={}, Type={}, Px={}, Qty={}, DecisionTS={}",
                  id_, req.client_order_id, req.symbol, (req.side == OrderSide::BUY ? "BUY" : "SELL"),
                  (req.type == OrderType::LIMIT ? "LIMIT" : "MARKET"), req.price, req.quantity, timestamp_to_string(req.request_timestamp));
        order_submitter_->submit_order_request(std::move(req));

        if(metrics_collector_) {
            metrics_collector_->record_latency(
                id_ + "_OrderSubmitted", 
                Duration(0), // Latency from what? This is just a marker.
                decision_ts 
            );
        }
    }

    // Specific handlers, called by on_event after visitation
    virtual void on_quote(const QuoteEvent& quote_event, Timestamp strategy_arrival_ts) = 0;
    virtual void on_trade(const TradeEvent& trade_event, Timestamp strategy_arrival_ts) = 0;
    virtual void on_order_ack(const OrderAckEvent& ack_event, Timestamp strategy_arrival_ts) = 0;
    virtual void on_sim_control(const SimControlEvent& control_event, Timestamp strategy_arrival_ts) = 0;


    StrategyId id_;
    IOrderSubmitter* order_submitter_; // Not owned
    std::shared_ptr<MetricsCollector> metrics_collector_; // Optional, shared
    OrderId next_client_order_id_;
};

// Visitor for StrategyInputEventVariant
struct StrategyEventVisitor {
    IStrategy& strategy_;
    Timestamp arrival_ts_; // The timestamp event arrived at strategy's queue processing point

    StrategyEventVisitor(IStrategy& strat, Timestamp ts) : strategy_(strat), arrival_ts_(ts) {}

    // Need to provide const and non-const overloads if unique_ptr content is modified.
    // Here, we pass const ref of event, so const unique_ptr<T>& is fine.
    void operator()(const std::unique_ptr<QuoteEvent>& ev) {
        static_cast<IStrategy&>(strategy_).on_quote(*ev, arrival_ts_);
    }
    void operator()(const std::unique_ptr<TradeEvent>& ev) {
        static_cast<IStrategy&>(strategy_).on_trade(*ev, arrival_ts_);
    }
    void operator()(const std::unique_ptr<OrderAckEvent>& ev) {
        static_cast<IStrategy&>(strategy_).on_order_ack(*ev, arrival_ts_);
    }
    void operator()(const std::unique_ptr<SimControlEvent>& ev) {
        static_cast<IStrategy&>(strategy_).on_sim_control(*ev, arrival_ts_);
    }
};


} // namespace market_replay
#endif // MARKET_REPLAY_STRATEGY_HPP