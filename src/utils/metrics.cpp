#include "market_replay/metrics.hpp"
#include "market_replay/common.hpp" // For timestamp_to_string
#include <iostream> // For std::cerr

namespace market_replay {

MetricsCollector::MetricsCollector(std::string trades_filepath, 
                                   std::string latency_filepath, 
                                   std::string pnl_filepath)
    : trades_filepath_(std::move(trades_filepath)),
      latency_filepath_(std::move(latency_filepath)),
      pnl_filepath_(std::move(pnl_filepath)) {
    LOG_INFO("MetricsCollector initialized. Trades: {}, Latency: {}, PnL: {}", 
             trades_filepath_, latency_filepath_, pnl_filepath_);
}

MetricsCollector::~MetricsCollector() {
    // report_final_metrics(); // Optionally report on destruction, but explicit call is better
}

void MetricsCollector::record_trade(const SimulatedTrade& trade) {
    std::lock_guard<std::mutex> lock(mtx_);
    trades_log_.push_back(trade);
    update_pnl(trade.strategy_id, trade.symbol, trade.price, trade.quantity, trade.side);
}

void MetricsCollector::record_latency(const std::string& source_desc, Duration latency, Timestamp event_time, const std::string& notes) {
    std::lock_guard<std::mutex> lock(mtx_);
    latency_log_.push_back({event_time, source_desc, latency, notes});
}

void MetricsCollector::update_pnl(const std::string& strategy_id, const std::string& symbol, Price fill_price, Quantity filled_quantity, OrderSide side) {
    // This is a simplified PnL calculation (no average price for inventory, just FIFO-like realized)
    // Proper PnL needs careful position tracking.
    auto& pnl_entry = pnl_by_strategy_symbol_[{strategy_id, symbol}];

    double trade_value = static_cast<double>(fill_price) * filled_quantity;
    pnl_entry.total_volume_traded += trade_value;

    if (side == OrderSide::BUY) {
        // If previously short, buying back reduces short position / realizes PnL
        if (pnl_entry.current_position < 0) {
            Quantity closed_qty = std::min(filled_quantity, static_cast<Quantity>(-pnl_entry.current_position));
            // PnL realized = (avg_short_price - fill_price) * closed_qty. Needs avg_short_price.
            // Simplified: for now, just track position. Detailed PnL is complex.
        }
        pnl_entry.current_position += filled_quantity;
    } else { // SELL
        // If previously long, selling reduces long position / realizes PnL
        if (pnl_entry.current_position > 0) {
            Quantity closed_qty = std::min(filled_quantity, static_cast<Quantity>(pnl_entry.current_position));
            // PnL realized = (fill_price - avg_long_price) * closed_qty. Needs avg_long_price.
        }
        pnl_entry.current_position -= filled_quantity;
    }
    // For now, just log that a PnL update happened. Real PnL is a project in itself.
    LOG_DEBUG("Metrics: PnL updated for Strategy {}, Symbol {}. Position: {}", strategy_id, symbol, pnl_entry.current_position);
}


void MetricsCollector::report_final_metrics() {
    std::lock_guard<std::mutex> lock(mtx_); // Ensure all data is consistent before writing
    LOG_INFO("MetricsCollector: Generating final reports...");
    write_trades_log();
    write_latency_log();
    write_pnl_summary(); // PnL summary will be very basic for now
    LOG_INFO("MetricsCollector: Final reports generated.");
}

void MetricsCollector::write_trades_log() {
    std::ofstream outfile(trades_filepath_);
    if (!outfile.is_open()) {
        LOG_ERROR("MetricsCollector: Failed to open trades log file: {}", trades_filepath_);
        return;
    }

    outfile << std::fixed << std::setprecision(5); // For prices
    outfile << "TimestampNS,StrategyID,Symbol,Side,Price,Quantity,ClientOrderID,ExchangeOrderID\n";
    for (const auto& trade : trades_log_) {
        outfile << timestamp_to_string(trade.timestamp) << ","
                << trade.strategy_id << ","
                << trade.symbol << ","
                << (trade.side == OrderSide::BUY ? "BUY" : "SELL") << ","
                << trade.price << ","
                << trade.quantity << ","
                << trade.client_order_id << ","
                << trade.exchange_order_id << "\n";
    }
    outfile.close();
    LOG_INFO("MetricsCollector: Trades log written to {}", trades_filepath_);
}

void MetricsCollector::write_latency_log() {
    std::ofstream outfile(latency_filepath_);
    if (!outfile.is_open()) {
        LOG_ERROR("MetricsCollector: Failed to open latency log file: {}", latency_filepath_);
        return;
    }
    
    outfile << "EventTimestampNS,SourceDescription,LatencyNS,Notes\n";
    for (const auto& rec : latency_log_) {
        outfile << timestamp_to_string(rec.event_time) << ","
                << rec.source_description << ","
                << rec.latency_value.count() << ","
                << rec.notes << "\n";
    }
    outfile.close();
    LOG_INFO("MetricsCollector: Latency log written to {}", latency_filepath_);
}

void MetricsCollector::write_pnl_summary() {
    std::ofstream outfile(pnl_filepath_);
     if (!outfile.is_open()) {
        LOG_ERROR("MetricsCollector: Failed to open PnL summary file: {}", pnl_filepath_);
        return;
    }

    outfile << std::fixed << std::setprecision(2);
    outfile << "StrategyID,Symbol,FinalPosition,TotalVolumeTraded,RealizedPnL(TODO),UnrealizedPnL(TODO)\n";
    for (const auto& entry : pnl_by_strategy_symbol_) {
        const auto& ids = entry.first; // pair<StrategyId, Symbol>
        const auto& pnl = entry.second;
        outfile << ids.first << ","  // Strategy ID
                << ids.second << "," // Symbol
                << pnl.current_position << ","
                << pnl.total_volume_traded << ","
                << pnl.realized_pnl << ","  // Will be 0 for now
                << pnl.unrealized_pnl << "\n"; // Will be 0 for now
    }
    outfile.close();
    LOG_INFO("MetricsCollector: PnL summary written to {}", pnl_filepath_);
}

} // namespace market_replay