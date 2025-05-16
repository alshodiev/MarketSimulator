#include "market_replay/logger.hpp"
#include "market_replay/dispatcher.hpp"
#include "market_replay/latency_model.hpp"
#include "market_replay/metrics.hpp"
// #include "market_replay/strategy.hpp" // BasicStrategy factory is forward declared or included by dispatcher

#include <iostream>
#include <string>
#include <memory> // For std::make_shared

// Forward declare strategy factory from basic_strategy.cpp
// This allows main to not directly depend on every strategy implementation file.
// The dispatcher can take a std::function for the factory.
namespace market_replay {
    std::unique_ptr<IStrategy> create_basic_strategy(const StrategyId& id,
                                                     IOrderSubmitter* order_submitter,
                                                     std::shared_ptr<MetricsCollector> metrics_collector);
}


int main(int argc, char* argv[]) {
    // --- Initialize Logger ---
    // Async true for performance in main sim, false for tests if easier to debug.
    market_replay::Logger::init("simulator_log.txt", spdlog::level::info, spdlog::level::debug, true); 
    LOG_INFO("Market Replay Simulator starting...");

    if (argc < 2) {
        LOG_CRITICAL("Usage: {} <path_to_tick_data.csv> [path_to_config.ini (optional)]", argv[0]);
        market_replay::Logger::shutdown();
        return 1;
    }
    std::string data_file_path = argv[1];
    LOG_INFO("Input data file: {}", data_file_path);

    // --- Configure Latency Model (Example fixed values, could load from config) ---
    market_replay::LatencyModel::Config latency_cfg;
    latency_cfg.market_data_feed_latency = market_replay::string_to_duration_ns("50us");
    latency_cfg.strategy_processing_latency = market_replay::string_to_duration_ns("5us");
    latency_cfg.order_network_latency_strat_to_exch = market_replay::string_to_duration_ns("20us");
    latency_cfg.exchange_order_processing_latency = market_replay::string_to_duration_ns("10us");
    latency_cfg.exchange_fill_processing_latency = market_replay::string_to_duration_ns("15us");
    latency_cfg.ack_network_latency_exch_to_strat = market_replay::string_to_duration_ns("20us");
    LOG_INFO("Latency model configured: MD Feed: {}ns, Strat Proc: {}ns, Order Net: {}ns, Exch Ack Proc: {}ns, Exch Fill Proc: {}ns, Ack Net: {}ns",
        latency_cfg.market_data_feed_latency.count(), latency_cfg.strategy_processing_latency.count(),
        latency_cfg.order_network_latency_strat_to_exch.count(), latency_cfg.exchange_order_processing_latency.count(),
        latency_cfg.exchange_fill_processing_latency.count(), latency_cfg.ack_network_latency_exch_to_strat.count());

    // --- Initialize Metrics Collector ---
    auto metrics_collector = std::make_shared<market_replay::MetricsCollector>(
        "sim_trades.csv", "sim_latency.csv", "sim_pnl.csv");
    LOG_INFO("Metrics collector initialized. Outputs: sim_trades.csv, sim_latency.csv, sim_pnl.csv");

    // --- Create Dispatcher ---
    try {
        market_replay::Dispatcher dispatcher(data_file_path, latency_cfg, metrics_collector);

        // --- Add Strategies ---
        // Using the factory function for BasicStrategy
        dispatcher.add_strategy("BasicStrat_EURUSD_1", market_replay::create_basic_strategy);
        // dispatcher.add_strategy("AnotherStrat_GBPUSD_1", some_other_strategy_factory);

        LOG_INFO("Dispatcher and strategies configured. Starting simulation run...");
        
        auto sim_start_time = std::chrono::high_resolution_clock::now();
        dispatcher.run(); // This blocks until simulation is complete
        auto sim_end_time = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(sim_end_time - sim_start_time);
        LOG_INFO("Simulation run finished in {} ms.", duration.count());

    } catch (const std::exception& e) {
        LOG_CRITICAL("Unhandled exception during simulation setup or run: {}", e.what());
        metrics_collector->report_final_metrics(); // Try to report what we have
        market_replay::Logger::shutdown();
        return 1;
    }
    
    // --- Report Metrics ---
    metrics_collector->report_final_metrics();

    LOG_INFO("Market Replay Simulator finished successfully.");
    market_replay::Logger::shutdown();
    return 0;
}