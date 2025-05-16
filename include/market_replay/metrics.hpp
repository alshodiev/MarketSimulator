#ifndef MARKET_REPLAY_METRICS_HPP
#define MARKET_REPLAY_METRICS_HPP

#include "common.hpp"
#include "logger.hpp" // For logging within metrics
#include <vector>
#include <fstream>
#include <iomanip> 
#include <string>
#include <map>
#include <mutex> // For thread safety

namespace market_replay {

struct SimulatedTrade {
    Timestamp timestamp; // Timestamp of fill ack arrival at strategy
    std::string strategy_id;
    std::string symbol;
    OrderSide side;
    Price price;
    Quantity quantity;
    OrderId client_order_id;
    OrderId exchange_order_id;
};

struct LatencyRecord {
    Timestamp event_time; // Timestamp when the latency was recorded/finalized
    std::string source_description; // e.g., "StrategyA_OrderDecision", "QuoteDispatch_EURUSD"
    Duration latency_value;
    std::string notes; // e.g., "MarketDataToStrategy", "OrderDecisionToFillAck"
};

class MetricsCollector {
public:
    MetricsCollector(std::string trades_filepath, std::string latency_filepath, std::string pnl_filepath);
    ~MetricsCollector();

    void record_trade(const SimulatedTrade& trade);
    void record_latency(const std::string& source_desc, Duration latency, Timestamp event_time, const std::string& notes = "");
    
    void update_pnl(const std::string& strategy_id, const std::string& symbol, Price fill_price, Quantity filled_quantity, OrderSide side);
    void report_final_metrics();

private:
    std::string trades_filepath_;
    std::string latency_filepath_;
    std::string pnl_filepath_;

    std::vector<SimulatedTrade> trades_log_;
    std::vector<LatencyRecord> latency_log_;
    std::map<std::pair<StrategyId, std::string>, PnL> pnl_by_strategy_symbol_; // {Strategy, Symbol} -> PnL

    mutable std::mutex mtx_; // Protects all shared data structures

    void write_trades_log();
    void write_latency_log();
    void write_pnl_summary();
};

} // namespace market_replay
#endif // MARKET_REPLAY_METRICS_HPP