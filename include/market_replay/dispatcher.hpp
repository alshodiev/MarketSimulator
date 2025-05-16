#ifndef MARKET_REPLAY_DISPATCHER_HPP
#define MARKET_REPLAY_DISPATCHER_HPP

#include "common.hpp"
#include "event.hpp"
#include "strategy.hpp"
#include "latency_model.hpp"
#include "metrics.hpp"
#include "csv_parser.hpp"
#include "interfaces.hpp" // For IOrderSubmitter
#include "utils/blocking_queue.hpp"
#include "order_book.hpp" // For simple matching

#include <vector>
#include <string>
#include <thread>
#include <map>
#include <queue> // For std::priority_queue
#include <atomic> // For atomic_bool, atomic OrderId

namespace market_replay {

// Factory function type for creating strategies
using StrategyFactory = std::function<std::unique_ptr<IStrategy>(
    const StrategyId& id,
    IOrderSubmitter* order_submitter,
    std::shared_ptr<MetricsCollector> metrics_collector
)>;


class Dispatcher : public IOrderSubmitter {
public:
    Dispatcher(std::string historical_data_path,
               LatencyModel::Config latency_config,
               std::shared_ptr<MetricsCollector> metrics_collector);
    ~Dispatcher();

    void add_strategy(const StrategyId& id, StrategyFactory factory);
    void run(); // Starts the simulation

    // --- IOrderSubmitter implementation ---
    void submit_order_request(OrderRequest request) override;

private:
    // --- Core Data Structures ---
    std::string historical_data_path_;
    LatencyModel latency_model_;
    std::shared_ptr<MetricsCollector> metrics_collector_;
    std::unique_ptr<CsvParser> csv_parser_;

    // Main Event Priority Queue (MEPQ) for time-ordered event processing by Dispatcher
    std::priority_queue<std::unique_ptr<BaseEvent>,
                        std::vector<std::unique_ptr<BaseEvent>>,
                        EventComparator> main_event_pq_;
    
    // Queue for order requests from strategies to dispatcher
    utils::BlockingQueue<OrderRequest> incoming_order_requests_;

    // Per-strategy resources
    struct StrategyRunner {
        StrategyId id;
        std::unique_ptr<IStrategy> strategy_instance;
        std::unique_ptr<utils::BlockingQueue<StrategyInputEventVariant>> input_queue;
        std::thread thread;

        StrategyRunner(StrategyId s_id, 
                       std::unique_ptr<IStrategy> inst,
                       std::unique_ptr<utils::BlockingQueue<StrategyInputEventVariant>> q)
            : id(std::move(s_id)), strategy_instance(std::move(inst)), input_queue(std::move(q)) {}
        
        // Need to define move constructor/assignment for vector of StrategyRunner
        StrategyRunner(StrategyRunner&& other) noexcept
            : id(std::move(other.id)),
              strategy_instance(std::move(other.strategy_instance)),
              input_queue(std::move(other.input_queue)),
              thread(std::move(other.thread)) {}

        StrategyRunner& operator=(StrategyRunner&& other) noexcept {
            if (this != &other) {
                if(thread.joinable()) thread.join(); // Join existing thread if any
                id = std::move(other.id);
                strategy_instance = std::move(other.strategy_instance);
                input_queue = std::move(other.input_queue);
                thread = std::move(other.thread);
            }
            return *this;
        }
        // Delete copy constructor and assignment
        StrategyRunner(const StrategyRunner&) = delete;
        StrategyRunner& operator=(const StrategyRunner&) = delete;
    };
    std::vector<StrategyRunner> strategy_runners_;
    std::map<StrategyId, StrategyRunner*> strategy_map_; // For quick lookup

    // Simulation state
    Timestamp current_simulation_time_; // Tracks the effective time of the event being processed by dispatcher
    std::atomic<bool> simulation_running_ = {false};
    std::atomic<OrderId> next_exchange_order_id_ = {1};

    // Simple order books per symbol for matching
    std::map<std::string, SimpleOrderBook> order_books_;

    // Active orders (e.g. passive limit orders) could be stored here if complex matching is needed.
    // For now, fills are immediate or based on next quote/trade.

    // --- Methods ---
    void dispatcher_thread_loop();
    void strategy_thread_loop(StrategyRunner* runner);

    void load_initial_data();
    void process_event_from_mepq(std::unique_ptr<BaseEvent> event);
    void handle_market_data_event(BaseEvent* event); // QuoteEvent or TradeEvent
    void handle_order_ack_event(OrderAckEvent* event);
    void handle_sim_control_event(SimControlEvent* event);
    
    void process_incoming_order_requests(); // Called periodically or triggered
    void simulate_order_lifecycle(OrderRequest order_req);

    OrderId get_next_exchange_order_id() { return next_exchange_order_id_++; }
    SimpleOrderBook& get_or_create_order_book(const std::string& symbol);

    void shutdown_strategies();
};

} // namespace market_replay
#endif // MARKET_REPLAY_DISPATCHER_HPP