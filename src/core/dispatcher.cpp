#include "market_replay/dispatcher.hpp"
#include "market_replay/logger.hpp"
#include <algorithm> // For std::find_if
#include <chrono>    // For std::this_thread::sleep_for

// Forward declare the factory from basic_strategy.cpp (or other strategy files)
namespace market_replay {
    std::unique_ptr<IStrategy> create_basic_strategy(const StrategyId& id,
                                                     IOrderSubmitter* order_submitter,
                                                     std::shared_ptr<MetricsCollector> metrics_collector);
}


namespace market_replay {

Dispatcher::Dispatcher(std::string historical_data_path,
                       LatencyModel::Config latency_config,
                       std::shared_ptr<MetricsCollector> metrics_collector)
    : historical_data_path_(std::move(historical_data_path)),
      latency_model_(latency_config),
      metrics_collector_(metrics_collector),
      current_simulation_time_(Timestamp::min()) { // Initialize to a very early time
    LOG_INFO("Dispatcher: Initialized. Data: {}, Latency configured.", historical_data_path_);
}

Dispatcher::~Dispatcher() {
    LOG_INFO("Dispatcher: Shutting down...");
    if (simulation_running_.load()) {
        simulation_running_.store(false); // Signal threads
    }
    // Shutdown queues to unblock threads
    incoming_order_requests_.shutdown();
    for (auto& runner : strategy_runners_) {
        if (runner.input_queue) {
            runner.input_queue->shutdown();
        }
        if (runner.thread.joinable()) {
            runner.thread.join();
        }
    }
    LOG_INFO("Dispatcher: All threads joined. Shutdown complete.");
}

void Dispatcher::add_strategy(const StrategyId& id, StrategyFactory factory) {
    if (simulation_running_.load()) {
        LOG_ERROR("Dispatcher: Cannot add strategy while simulation is running.");
        return;
    }
    auto input_queue = std::make_unique<utils::BlockingQueue<StrategyInputEventVariant>>(10000); // Bounded queue
    
    // The factory creates the strategy instance
    auto strategy_instance = factory(id, this, metrics_collector_);
    if (!strategy_instance) {
        LOG_ERROR("Dispatcher: Failed to create strategy with ID: {}", id);
        return;
    }

    strategy_runners_.emplace_back(id, std::move(strategy_instance), std::move(input_queue));
    strategy_map_[id] = &strategy_runners_.back();
    LOG_INFO("Dispatcher: Added strategy '{}'", id);
}

void Dispatcher::strategy_thread_loop(StrategyRunner* runner) {
    LOG_INFO("Strategy Thread [{}]: Starting.", runner->id);
    if (!runner->strategy_instance) {
        LOG_ERROR("Strategy Thread [{}]: Strategy instance is null. Exiting.", runner->id);
        return;
    }

    runner->strategy_instance->on_init(current_simulation_time_); // Pass current dispatcher time

    StrategyInputEventVariant event_variant;
    while (simulation_running_.load() && runner->input_queue->wait_and_pop(event_variant)) {
        Timestamp event_arrival_ts = Timestamp::min(); // This should be the effective_timestamp of the event
        
        // Determine the effective arrival timestamp from the variant
        std::visit([&event_arrival_ts](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (!std::is_same_v<T, std::monostate>) { // Check if it's not an empty variant
                 if (arg) event_arrival_ts = arg->get_effective_timestamp();
            }
        }, event_variant);

        if(event_arrival_ts == Timestamp::min() && !runner->input_queue->is_shutdown()){
            LOG_WARN("Strategy Thread [{}]: Popped an event variant with no valid event or timestamp.", runner->id);
            continue; // Skip if event is somehow invalid
        }
        
        // Check for shutdown signal (a specific SimControlEvent or just queue shutdown)
        bool is_shutdown_event = false;
        if (auto* p_sc_event = std::get_if<std::unique_ptr<SimControlEvent>>(&event_variant)) {
            if (*p_sc_event && (*p_sc_event)->control_type == SimControlEvent::ControlType::STRATEGY_SHUTDOWN) {
                is_shutdown_event = true;
            }
        }

        if (is_shutdown_event) {
            LOG_INFO("Strategy Thread [{}]: Received shutdown signal via event. Exiting loop.", runner->id);
            break; 
        }
        
        // Record latency: time from event's scheduled arrival to when strategy starts processing
        // This would be: actual_processing_start_time (now) - event_arrival_ts
        // For now, assume they are close if strategy thread is not backlogged.
        if (metrics_collector_ && event_arrival_ts != Timestamp::min()) {
            auto now = std::chrono::system_clock::now(); // Approximation of processing start
            // This metric is tricky because event_arrival_ts is from the *simulation* clock.
            // True strategy processing latency relative to sim clock:
            // Need strategy to record its own current_sim_time when it starts processing.
            // For now, we use event_arrival_ts as the "official" time.
        }
        
        runner->strategy_instance->on_event(event_variant, event_arrival_ts);
    }
    
    runner->strategy_instance->on_shutdown(current_simulation_time_);
    LOG_INFO("Strategy Thread [{}]: Exited event loop and shut down.", runner->id);
}


void Dispatcher::load_initial_data() {
    LOG_INFO("Dispatcher: Loading historical data from {}", historical_data_path_);
    csv_parser_ = std::make_unique<CsvParser>(historical_data_path_);
    
    int count = 0;
    while (csv_parser_->has_more_events()) {
        std::unique_ptr<BaseEvent> market_event = csv_parser_->read_next_event();
        if (market_event) {
            // Apply latency for market data arrival at strategy
            Duration md_latency = latency_model_.get_market_data_latency(*market_event);
            market_event->arrival_timestamp = market_event->exchange_timestamp + md_latency;
            
            main_event_pq_.push(std::move(market_event));
            count++;
        }
    }
    LOG_INFO("Dispatcher: Loaded {} market events into MEPQ.", count);

    if (!main_event_pq_.empty()) {
         // Add a SimControlEvent to mark the end of the data feed, scheduled after the last market data event.
        Timestamp last_event_arrival_ts = main_event_pq_.top()->get_effective_timestamp(); // This is sorted by arrival at strategy
        // Actually, we need last exchange timestamp to schedule correctly
        // This requires iterating or storing last exchange_ts during load.
        // For now, schedule it slightly after the last known effective arrival.
        // A better way: when CsvParser returns nullptr for the first time, then schedule END_OF_DATA_FEED.
        // This is handled in the main loop now.
    } else {
        LOG_WARN("Dispatcher: No market data loaded. Simulation might be empty.");
        // Schedule an immediate end of data if nothing loaded.
        auto end_event = std::make_unique<SimControlEvent>(
            std::chrono::system_clock::now(), // Or some logical start time
            SimControlEvent::ControlType::END_OF_DATA_FEED);
        main_event_pq_.push(std::move(end_event));
    }
}

void Dispatcher::run() {
    if (strategy_runners_.empty()) {
        LOG_WARN("Dispatcher: No strategies added. Running simulation without strategies.");
    }
    simulation_running_.store(true);

    // Start strategy threads
    for (auto& runner : strategy_runners_) {
        LOG_INFO("Dispatcher: Starting thread for strategy '{}'", runner.id);
        runner.thread = std::thread(&Dispatcher::strategy_thread_loop, this, &runner);
    }

    // Load data (this will populate MEPQ)
    load_initial_data();

    // Add a periodic event to process incoming order requests
    // This ensures order requests are handled even if no market data is flowing.
    if (!main_event_pq_.empty()) {
        Timestamp next_check_time = main_event_pq_.top()->get_effective_timestamp();
        auto order_proc_event = std::make_unique<SimControlEvent>(
            next_check_time, SimControlEvent::ControlType::PROCESS_ORDER_REQUESTS);
        main_event_pq_.push(std::move(order_proc_event));
    } else { // If no market data, prime with a process order request event now
         auto order_proc_event = std::make_unique<SimControlEvent>(
            std::chrono::system_clock::now(), // Effectively now
            SimControlEvent::ControlType::PROCESS_ORDER_REQUESTS);
        main_event_pq_.push(std::move(order_proc_event));
    }


    LOG_INFO("Dispatcher: Starting main event loop.");
    bool data_feed_ended_signal_pushed = false;

    while (simulation_running_.load()) {
        process_incoming_order_requests(); // Process any pending orders quickly

        if (main_event_pq_.empty()) {
            if (incoming_order_requests_.empty() && !data_feed_ended_signal_pushed) {
                 // All market data processed, no pending orders, and no outstanding acks in MEPQ.
                LOG_INFO("Dispatcher: MEPQ and OrderRequestQueue are empty. Signaling end of data feed to strategies.");
                auto end_event = std::make_unique<SimControlEvent>(
                    current_simulation_time_ + Duration(1), // Slightly after current time
                    SimControlEvent::ControlType::END_OF_DATA_FEED,
                    EventType::SIM_CONTROL_STRATEGY); // This type will be for strategies
                
                // Push this control event to MEPQ, so it gets dispatched to strategies in time order.
                // Or directly push to strategies if MEPQ is truly done with other events.
                // For consistency, push to MEPQ.
                main_event_pq_.push(std::move(end_event));
                data_feed_ended_signal_pushed = true; // ensure this is done once
            } else if (incoming_order_requests_.empty() && data_feed_ended_signal_pushed) {
                 // If still empty after end_of_data_feed was pushed and presumably processed,
                 // then it's time to break.
                 LOG_INFO("Dispatcher: MEPQ is empty after END_OF_DATA_FEED processed. Ending simulation loop.");
                 break;
            }
            // If there are still order requests, the loop continues, process_incoming_order_requests will
            // generate new OrderAckEvents for MEPQ.
            // Sleep a bit to avoid busy-waiting if MEPQ is temporarily empty but sim not over.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        std::unique_ptr<BaseEvent> current_event_ptr;
        // Critical section for accessing MEPQ top & pop
        // No, MEPQ is only accessed by this thread.
        current_event_ptr = std::move(const_cast<std::unique_ptr<BaseEvent>&>(main_event_pq_.top()));
        main_event_pq_.pop();
        
        if (!current_event_ptr) {
            LOG_WARN("Dispatcher: Popped a nullptr from MEPQ. Skipping.");
            continue;
        }

        current_simulation_time_ = current_event_ptr->get_effective_timestamp();
        
        // Log the event being dispatched
        // LOG_TRACE("Dispatcher: Processing event type {} at sim_time {}", 
        //           static_cast<int>(current_event_ptr->type), timestamp_to_string(current_simulation_time_));

        process_event_from_mepq(std::move(current_event_ptr));

        // If CsvParser is exhausted and not yet signaled, add END_OF_DATA_FEED to MEPQ
        if (csv_parser_ && !csv_parser_->has_more_events() && !data_feed_ended_signal_pushed) {
            LOG_INFO("Dispatcher: CsvParser has no more events. Scheduling END_OF_DATA_FEED.");
             auto end_event = std::make_unique<SimControlEvent>(
                current_simulation_time_ + Duration(1), // Schedule it just after current event
                SimControlEvent::ControlType::END_OF_DATA_FEED,
                EventType::SIM_CONTROL_STRATEGY); 
            main_event_pq_.push(std::move(end_event));
            data_feed_ended_signal_pushed = true;
        }
    }

    LOG_INFO("Dispatcher: Main event loop finished.");
    simulation_running_.store(false); // Ensure it's set for any cleanup logic
    shutdown_strategies();
    LOG_INFO("Dispatcher: Run completed.");
}

void Dispatcher::process_event_from_mepq(std::unique_ptr<BaseEvent> event) {
    if (!event) return;

    // Update order books if it's a market data event BEFORE dispatching to strategies
    if (event->type == EventType::QUOTE) {
        auto* q_event = static_cast<QuoteEvent*>(event.get());
        get_or_create_order_book(q_event->symbol).update_quote(*q_event);
    } else if (event->type == EventType::TRADE) {
        // auto* t_event = static_cast<TradeEvent*>(event.get());
        // Potentially update last trade price in order book here if needed for matching logic
    }


    switch (event->type) {
        case EventType::QUOTE:
        case EventType::TRADE:
            handle_market_data_event(event.get());
            break;
        case EventType::ORDER_ACK:
            handle_order_ack_event(static_cast<OrderAckEvent*>(event.get()));
            break;
        case EventType::SIM_CONTROL_DISPATCHER:
        case EventType::SIM_CONTROL_STRATEGY: // Strategy control events also go through MEPQ
            handle_sim_control_event(static_cast<SimControlEvent*>(event.get()));
            break;
        default:
            LOG_WARN("Dispatcher: Unknown event type {} in MEPQ.", static_cast<int>(event->type));
            break;
    }
}

void Dispatcher::handle_market_data_event(BaseEvent* event) {
    // This event's arrival_timestamp is when it should reach the strategy.
    // current_simulation_time_ is already set to this.
    StrategyInputEventVariant event_variant;
    if (event->type == EventType::QUOTE) {
        // Need to copy, as multiple strategies might consume it.
        // Or use shared_ptr if events are large and copying is too much.
        // For now, unique_ptr and make_unique for each strategy queue.
        auto q_event = static_cast<QuoteEvent*>(event);
        // LOG_DEBUG("Dispatching Quote {} to strategies at {}", q_event->symbol, timestamp_to_string(q_event->arrival_timestamp));
        for (auto& runner : strategy_runners_) {
            auto q_copy = std::make_unique<QuoteEvent>(*q_event); // Copy
            q_copy->arrival_timestamp = event->arrival_timestamp; // Ensure arrival time is propagated
            runner.input_queue->push(std::move(q_copy));
        }
    } else if (event->type == EventType::TRADE) {
        auto t_event = static_cast<TradeEvent*>(event);
        // LOG_DEBUG("Dispatching Trade {} to strategies at {}", t_event->symbol, timestamp_to_string(t_event->arrival_timestamp));
        for (auto& runner : strategy_runners_) {
            auto t_copy = std::make_unique<TradeEvent>(*t_event); // Copy
            t_copy->arrival_timestamp = event->arrival_timestamp;
            runner.input_queue->push(std::move(t_copy));
        }
    }
}

void Dispatcher::handle_order_ack_event(OrderAckEvent* event) {
    // Route to the specific strategy
    // LOG_DEBUG("Dispatching OrderAck for ClientID {} (Strategy {}) at {}", event->client_order_id, event->strategy_id, timestamp_to_string(event->arrival_timestamp));
    auto it = strategy_map_.find(event->strategy_id);
    if (it != strategy_map_.end()) {
        auto ack_copy = std::make_unique<OrderAckEvent>(*event); // Copy
        ack_copy->arrival_timestamp = event->arrival_timestamp;
        it->second->input_queue->push(std::move(ack_copy));
    } else {
        LOG_WARN("Dispatcher: No strategy found for ID '{}' to send OrderAck.", event->strategy_id);
    }
}

void Dispatcher::handle_sim_control_event(SimControlEvent* event) {
    LOG_DEBUG("Dispatcher: Processing SimControlEvent type {} at {}", static_cast<int>(event->control_type), timestamp_to_string(event->arrival_timestamp));
    if (event->type == EventType::SIM_CONTROL_DISPATCHER) {
        if (event->control_type == SimControlEvent::ControlType::PROCESS_ORDER_REQUESTS) {
            process_incoming_order_requests();
            // Schedule next check
            if (simulation_running_.load() && !main_event_pq_.empty()) { // Only reschedule if sim is ongoing and there are future events
                Timestamp next_check_time = current_simulation_time_ + std::chrono::milliseconds(10); // Check every 10ms of sim time
                 // Ensure it's not scheduled past the next genuine event if that's sooner.
                if (!main_event_pq_.empty() && next_check_time > main_event_pq_.top()->get_effective_timestamp()){
                    // If next MEPQ event is sooner, schedule this check just before or at that time.
                    // This logic can be complex. A simpler way: process orders at start of each event loop iteration.
                    // For now, fixed interval from current time, or rely on loop start.
                } else {
                    // If main_event_pq is empty, this periodic check will keep it alive until orders stop.
                }
                auto next_proc_event = std::make_unique<SimControlEvent>(
                    next_check_time, SimControlEvent::ControlType::PROCESS_ORDER_REQUESTS);
                main_event_pq_.push(std::move(next_proc_event));
            }
        }
    } else if (event->type == EventType::SIM_CONTROL_STRATEGY) {
         if (event->control_type == SimControlEvent::ControlType::END_OF_DATA_FEED) {
            LOG_INFO("Dispatcher: END_OF_DATA_FEED detected by dispatcher. Signaling strategies to prepare for shutdown.");
            // This event, when processed from MEPQ, means all prior market data and acks are done.
            // Now, tell strategies they can wind down.
            for (auto& runner : strategy_runners_) {
                auto shutdown_signal = std::make_unique<SimControlEvent>(
                    event->arrival_timestamp, // Deliver at this same sim time
                    SimControlEvent::ControlType::STRATEGY_SHUTDOWN,
                    EventType::SIM_CONTROL_STRATEGY
                );
                runner.input_queue->push(std::move(shutdown_signal));
            }
        }
    }
}


void Dispatcher::submit_order_request(OrderRequest request) {
    // This is called by a strategy thread. Queue it for the dispatcher thread.
    // The request.request_timestamp is when the strategy made the decision.
    LOG_DEBUG("Dispatcher: Received order request from Strategy {} (ClientID {}) for {} {} @ {} Qty {}. DecisionTS: {}",
              request.strategy_id, request.client_order_id, (request.side == OrderSide::BUY ? "BUY" : "SELL"),
              request.symbol, request.price, request.quantity, timestamp_to_string(request.request_timestamp));
    incoming_order_requests_.push(std::move(request));
}

void Dispatcher::process_incoming_order_requests() {
    std::optional<OrderRequest> opt_req;
    while ((opt_req = incoming_order_requests_.try_pop()).has_value()) {
        simulate_order_lifecycle(std::move(opt_req.value()));
    }
}

void Dispatcher::simulate_order_lifecycle(OrderRequest order_req) {
    LOG_INFO("Dispatcher: Simulating lifecycle for order ClientID {} from Strategy {}", order_req.client_order_id, order_req.strategy_id);

    OrderId exchange_order_id = get_next_exchange_order_id();
    Timestamp decision_ts = order_req.request_timestamp; // When strategy decided.

    // Add strategy's own processing delay before it "sends" the order
    Timestamp order_effectively_sent_ts = decision_ts + latency_model_.get_strategy_processing_latency();
    
    Timestamp order_arrival_at_exchange_ts = latency_model_.get_order_arrival_at_exchange_ts(order_effectively_sent_ts);

    // 1. Send PENDING_NEW or NEW ack (acknowledging receipt by exchange system)
    Timestamp ack_arrival_strat_ts = latency_model_.get_ack_arrival_at_strategy_ts(order_arrival_at_exchange_ts);
    
    auto new_ack = std::make_unique<OrderAckEvent>(
        ack_arrival_strat_ts, order_req.strategy_id, order_req.client_order_id, exchange_order_id,
        order_req.symbol, OrderStatus::ACKNOWLEDGED // Or NEW, depending on model
    );
    new_ack->leaves_quantity = order_req.quantity; // Initially, all leaves
    main_event_pq_.push(std::move(new_ack));
    LOG_DEBUG("Dispatcher: Scheduled ACKNOWLEDGED for ClientID {} at {}", order_req.client_order_id, timestamp_to_string(ack_arrival_strat_ts));

    // 2. Simulate fill attempt
    // Fill simulation needs the state of the order book AT order_arrival_at_exchange_ts.
    // This is a simplification: we use the book state at current_simulation_time_ when this
    // order_req is processed. This assumes order_req processing is timely.
    // A more accurate simulation would snapshot the book or queue order processing against book states.
    
    SimpleOrderBook& book = get_or_create_order_book(order_req.symbol);
    Price fill_price = INVALID_PRICE;
    Quantity filled_qty = 0;

    if (order_req.type == OrderType::MARKET) {
        std::tie(fill_price, filled_qty) = book.match_market_order(order_req.side, order_req.quantity);
    } else if (order_req.type == OrderType::LIMIT) {
        std::tie(fill_price, filled_qty) = book.match_limit_order(order_req.side, order_req.price, order_req.quantity);
    }

    if (filled_qty > 0 && !std::isnan(fill_price)) {
        Timestamp fill_arrival_strat_ts = latency_model_.get_fill_arrival_at_strategy_ts(order_arrival_at_exchange_ts);
        // Ensure fill ack arrives after or at same time as initial ack
        if (fill_arrival_strat_ts < ack_arrival_strat_ts) {
             fill_arrival_strat_ts = ack_arrival_strat_ts + Duration(1); // Ensure causality
        }

        auto fill_ack = std::make_unique<OrderAckEvent>(
            fill_arrival_strat_ts, order_req.strategy_id, order_req.client_order_id, exchange_order_id,
            order_req.symbol, (filled_qty == order_req.quantity ? OrderStatus::FILLED : OrderStatus::PARTIALLY_FILLED)
        );
        fill_ack->last_filled_price = fill_price;
        fill_ack->last_filled_quantity = filled_qty;
        fill_ack->cumulative_filled_quantity = filled_qty; // Assuming no prior partial fills for this simple model
        fill_ack->leaves_quantity = order_req.quantity - filled_qty;
        main_event_pq_.push(std::move(fill_ack));
        LOG_DEBUG("Dispatcher: Scheduled FILL for ClientID {} ({} units at {}) at {}", 
                  order_req.client_order_id, filled_qty, fill_price, timestamp_to_string(fill_arrival_strat_ts));

        if (metrics_collector_) {
            metrics_collector_->record_latency(
                order_req.strategy_id + "_OrderFillAckLatency",
                fill_arrival_strat_ts - decision_ts, // Total latency from decision to fill ack
                fill_arrival_strat_ts
            );
        }

    } else if (order_req.type == OrderType::LIMIT) {
        // Passive limit order, no immediate fill. It's now "on the book".
        // Our simple model doesn't keep a full passive book for matching against future ticks.
        // It's just ACKNOWLEDGED. For it to fill, a future QUOTE or TRADE would need to cross it,
        // and we'd need logic here or in `handle_market_data_event` to check active limit orders.
        // This is out of scope for a basic implementation.
        LOG_INFO("Dispatcher: Limit order ClientID {} for {} is passive, no immediate fill.", order_req.client_order_id, order_req.symbol);
    } else { // Market order that couldn't fill (e.g. no liquidity)
        LOG_WARN("Dispatcher: Market order ClientID {} for {} could not be filled (Qty: {}). This might mean it's rejected or remains pending.",
            order_req.client_order_id, order_req.symbol, order_req.quantity);
        // Could send a REJECTED ack here or model it differently.
        // For now, it's just ACKNOWLEDGED and not filled.
    }
}


SimpleOrderBook& Dispatcher::get_or_create_order_book(const std::string& symbol) {
    auto it = order_books_.find(symbol);
    if (it == order_books_.end()) {
        LOG_INFO("Dispatcher: Creating new order book for symbol {}", symbol);
        it = order_books_.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(symbol),
                                  std::forward_as_tuple(symbol)).first;
    }
    return it->second;
}

void Dispatcher::shutdown_strategies() {
    LOG_INFO("Dispatcher: Initiating shutdown of strategy threads...");
    // Signal strategy threads to shutdown via their queues using a special event
    // This is now handled by the END_OF_DATA_FEED -> STRATEGY_SHUTDOWN chain
    // This function mostly ensures queues are shutdown and threads are joined.

    for (auto& runner : strategy_runners_) {
        if (runner.input_queue && !runner.input_queue->is_shutdown()) {
            // Send one final explicit shutdown if not already done through event.
            // This is a fallback.
            auto final_shutdown_signal = std::make_unique<SimControlEvent>(
                current_simulation_time_ + Duration(1), // Effectively now
                SimControlEvent::ControlType::STRATEGY_SHUTDOWN,
                EventType::SIM_CONTROL_STRATEGY
            );
            runner.input_queue->push(std::move(final_shutdown_signal));
            runner.input_queue->shutdown(); // Then hard shutdown the queue
        }
    }

    for (auto& runner : strategy_runners_) {
        if (runner.thread.joinable()) {
            LOG_DEBUG("Dispatcher: Joining thread for strategy '{}'...", runner.id);
            runner.thread.join();
            LOG_INFO("Dispatcher: Joined thread for strategy '{}'.", runner.id);
        }
    }
    LOG_INFO("Dispatcher: All strategy threads have been joined.");
}


} // namespace market_replay