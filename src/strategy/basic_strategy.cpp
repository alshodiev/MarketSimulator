#include "market_replay/strategy.hpp"
#include "market_replay/logger.hpp" // Already in strategy.hpp but good practice
#include <iostream> // For console output from strategy if needed

namespace market_replay {

class BasicStrategy : public IStrategy {
public:
    BasicStrategy(StrategyId id, 
                  IOrderSubmitter* order_submitter, 
                  std::shared_ptr<MetricsCollector> metrics_collector)
        : IStrategy(std::move(id), order_submitter, metrics_collector) {}

    void on_init(Timestamp current_sim_time) override {
        LOG_INFO("Strategy [{}]: Initialized at sim time {}", id_, timestamp_to_string(current_sim_time));
        // Example: submit an initial order if needed (though usually based on market data)
    }

    void on_event(const StrategyInputEventVariant& event_variant, Timestamp strategy_arrival_ts) override {
        // The arrival_ts is when this event was popped from the strategy's queue.
        // The event itself (e.g. QuoteEvent) also has an arrival_timestamp which is when it *should* have been seen.
        // There might be a delta if strategy thread was busy.
        
        // For metrics: strategy reaction latency = strategy_arrival_ts - original_event.arrival_timestamp
        // (Assuming original_event.arrival_timestamp is when it was pushed to strategy queue)
        // For simplicity, we'll assume strategy_arrival_ts IS the event's intended arrival time.

        std::visit(StrategyEventVisitor(*this, strategy_arrival_ts), event_variant);
    }

    void on_quote(const QuoteEvent& quote, Timestamp strategy_arrival_ts) override {
        LOG_DEBUG("Strategy [{}]: Received Quote: Symbol={}, BidPx={}, BidSz={}, AskPx={}, AskSz={}, ArrivalTS={}",
                  id_, quote.symbol, quote.bid_price, quote.bid_size, quote.ask_price, quote.ask_size,
                  timestamp_to_string(strategy_arrival_ts));

        // Example: Simple Market Order on first quote for "EURUSD"
        if (quote.symbol == "EURUSD" && !eurusd_order_sent_ && quote.ask_price > 0 && quote.ask_size > 0) {
            LOG_INFO("Strategy [{}]: EURUSD quote received, submitting market buy order.", id_);
            // Decision time is the arrival time of the quote that triggered it.
            // Plus any strategy_processing_latency modeled if not done by LatencyModel globally.
            // Here, decision_ts is the quote's arrival time.
            submit_order(quote.symbol, OrderSide::BUY, OrderType::MARKET, INVALID_PRICE, 1000, strategy_arrival_ts);
            eurusd_order_sent_ = true;
        }
    }

    void on_trade(const TradeEvent& trade, Timestamp strategy_arrival_ts) override {
        LOG_DEBUG("Strategy [{}]: Received Trade: Symbol={}, Price={}, Size={}, ArrivalTS={}",
                  id_, trade.symbol, trade.price, trade.size, timestamp_to_string(strategy_arrival_ts));
    }

// Inside BasicStrategy class, in on_order_ack method:
    void on_order_ack(const OrderAckEvent& ack, Timestamp strategy_arrival_ts) override {
        LOG_INFO("Strategy [{}]: Received OrderAck: ClientID={}, ExchID={}, Symbol={}, Status={}, LastFillPx={}, LastFillQty={}, CumQty={}, Leaves={}, ArrivalTS={}",
                 id_, ack.client_order_id, ack.exchange_order_id, ack.symbol, static_cast<int>(ack.status),
                 ack.last_filled_price, ack.last_filled_quantity, ack.cumulative_filled_quantity, ack.leaves_quantity,
                 timestamp_to_string(strategy_arrival_ts));
        
        if (ack.status == OrderStatus::REJECTED) {
            LOG_ERROR("Strategy [{}]: Order ClientID={} was REJECTED: {}", id_, ack.client_order_id, ack.reject_reason);
        }

        if (metrics_collector_ && 
            (ack.status == OrderStatus::FILLED || ack.status == OrderStatus::PARTIALLY_FILLED) && 
            ack.last_filled_quantity > 0) {
            
            OrderSide determined_trade_side;

            // --- Simplified logic to determine trade side for BasicStrategy ---
            // This strategy only sends one specific BUY order for EURUSD.
            // We assume any fill for EURUSD corresponding to our known client_order_id range 
            // (or specifically if client_order_id == 1, since it's the first) is that BUY.
            // This is still a simplification. A real strategy would store details of all sent orders.
            if (ack.symbol == "EURUSD" && eurusd_order_sent_ && ack.client_order_id == 1) { // Assuming client_order_id 1 was the EURUSD buy
                determined_trade_side = OrderSide::BUY; 
            } else {
                // If we can't determine (e.g., other symbols, or other orders if strategy was more complex)
                LOG_WARN("Strategy [{}]: Could not reliably determine original side for filled order ClientID={}. Defaulting to BUY for metrics as a placeholder.", id_, ack.client_order_id);
                determined_trade_side = OrderSide::BUY; // Placeholder - THIS IS NOT ROBUST for a general strategy
            }
            // --- End logic to determine trade side ---

            SimulatedTrade simulated_trade{
                /*timestamp*/         strategy_arrival_ts,
                /*strategy_id*/       id_, 
                /*symbol*/            ack.symbol,
                /*side*/              determined_trade_side,
                /*price*/             ack.last_filled_price,
                /*quantity*/          ack.last_filled_quantity,
                /*client_order_id*/   ack.client_order_id,
                /*exchange_order_id*/ ack.exchange_order_id
            };
            metrics_collector_->record_trade(simulated_trade);
        }
    }

    void on_sim_control(const SimControlEvent& control_event, Timestamp strategy_arrival_ts) override {
        LOG_INFO("Strategy [{}]: Received SimControlEvent Type={} at {}", 
                 id_, static_cast<int>(control_event.control_type), timestamp_to_string(strategy_arrival_ts));
        if (control_event.control_type == SimControlEvent::ControlType::STRATEGY_SHUTDOWN) {
            LOG_INFO("Strategy [{}]: Shutdown signal received.", id_);
            // Perform any cleanup before thread exits
        }
    }


    void on_shutdown(Timestamp current_sim_time) override {
        LOG_INFO("Strategy [{}]: Shutting down at sim time {}", id_, timestamp_to_string(current_sim_time));
        // Report strategy-specific PnL, etc.
    }

private:
    bool eurusd_order_sent_ = false;
};

class MeanReversionStrategy : public IStrategy {
public:
    MeanReversionStrategy(StrategyId id, 
                  IOrderSubmitter* order_submitter, 
                  std::shared_ptr<MetricsCollector> metrics_collector)
        : IStrategy(std::move(id), order_submitter, metrics_collector) {}

    void on_init(Timestamp /*current_sim_time*/) override {
        LOG_INFO("Strategy [{}]: Mean Reversion Initialized.", id_);
    }

    void on_event(const StrategyInputEventVariant& event_variant, Timestamp strategy_arrival_ts) override {
        std::visit(StrategyEventVisitor(*this, strategy_arrival_ts), event_variant);
    }

    void on_quote(const QuoteEvent& quote, Timestamp strategy_arrival_ts) override {
        LOG_DEBUG("MeanRev Strat [{}]: Quote: Symbol={}, BidPx={}, AskPx={}, ArrivalTS={}",
                  id_, quote.symbol, quote.bid_price, quote.ask_price, timestamp_to_string(strategy_arrival_ts));
        // TODO: Implement actual mean reversion logic
        // For now, let's make it submit a trade on EURUSD differently than BasicStrategy
        if (quote.symbol == "EURUSD" && !order_sent_ && quote.bid_price > 0 && quote.bid_size > 0) {
            LOG_INFO("MeanRev Strat [{}]: EURUSD quote, submitting market SELL.", id_);
            submit_order(quote.symbol, OrderSide::SELL, OrderType::MARKET, INVALID_PRICE, 500, strategy_arrival_ts);
            order_sent_ = true;
        }
    }
    void on_trade(const TradeEvent& trade, Timestamp strategy_arrival_ts) override { /* ... */ }
    void on_order_ack(const OrderAckEvent& ack, Timestamp strategy_arrival_ts) override {
        LOG_INFO("MeanRev Strat [{}]: OrderAck: ClientID={}, Status={}", id_, ack.client_order_id, static_cast<int>(ack.status));
        // Simplified: just log. Metrics recording would be similar to BasicStrategy
        if (metrics_collector_ && (ack.status == OrderStatus::FILLED || ack.status == OrderStatus::PARTIALLY_FILLED) && ack.last_filled_quantity > 0) {
             SimulatedTrade simulated_trade{
                strategy_arrival_ts, id_, ack.symbol,
                OrderSide::SELL, // Assuming this strategy only sells for now
                ack.last_filled_price, ack.last_filled_quantity,
                ack.client_order_id, ack.exchange_order_id
            };
            metrics_collector_->record_trade(simulated_trade);
        }
    }
    void on_sim_control(const SimControlEvent& /*control_event*/, Timestamp /*strategy_arrival_ts*/) override {}
    void on_shutdown(Timestamp /*current_sim_time*/) override {
        LOG_INFO("Strategy [{}]: Mean Reversion Shutting down.", id_);
    }
private:
    bool order_sent_ = false;
};

std::unique_ptr<IStrategy> create_basic_strategy(const StrategyId& id,
                                                 IOrderSubmitter* order_submitter,
                                                 std::shared_ptr<MetricsCollector> metrics_collector) {
    return std::make_unique<BasicStrategy>(id, order_submitter, metrics_collector);
}

std::unique_ptr<IStrategy> create_mean_reversion_strategy(const StrategyId& id,
                                                         IOrderSubmitter* order_submitter,
                                                         std::shared_ptr<MetricsCollector> metrics_collector) {
    return std::make_unique<MeanReversionStrategy>(id, order_submitter, metrics_collector);
}



} // namespace market_replay