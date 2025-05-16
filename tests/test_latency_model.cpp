#include "catch2/catch_test_macros.hpp"
#include "test_utils.hpp" 
#include "market_replay/latency_model.hpp"
#include "market_replay/event.hpp" // For dummy event

using namespace std::chrono_literals;

TEST_CASE("LatencyModel Calculations", "[latency_model]") {
    market_replay::LatencyModel::Config config;
    config.market_data_feed_latency = 100us;
    config.strategy_processing_latency = 10us;
    config.order_network_latency_strat_to_exch = 50us;
    config.exchange_order_processing_latency = 20us;
    config.exchange_fill_processing_latency = 30us; // Slightly longer for fill
    config.ack_network_latency_exch_to_strat = 50us;

    market_replay::LatencyModel model(config);
    market_replay::Timestamp t0 = market_replay::Timestamp(0ns); // Epoch for easy calculation

    SECTION("Market Data Latency") {
        market_replay::QuoteEvent dummy_quote(t0, "SYM", 1.0, 100, 1.1, 100);
        REQUIRE(model.get_market_data_latency(dummy_quote) == 100us);
    }

    SECTION("Strategy Processing Latency") {
        REQUIRE(model.get_strategy_processing_latency() == 10us);
    }

    SECTION("Order Arrival at Exchange") {
        // strategy_decision_ts is t0.
        // This function expects decision_ts to already include strategy processing time.
        // So if strategy decides at t0, it effectively sends at t0.
        // The latency model adds network latency on top.
        market_replay::Timestamp decision_ts = t0; // Strategy code calls submit() at t0
        // The dispatcher would add strategy_processing_latency BEFORE calling this, or this model does.
        // My model's get_order_arrival_at_exchange_ts assumes input is strategy_decision_ts
        // and adds order_network_latency_strat_to_exch.
        // If strategy_processing_latency is to be added, it's typically before this call.
        // Let's assume strategy_decision_ts is the point AFTER internal processing.

        market_replay::Timestamp expected_arrival_at_exchange = decision_ts + config.order_network_latency_strat_to_exch;
        REQUIRE(model.get_order_arrival_at_exchange_ts(decision_ts) == expected_arrival_at_exchange);
        REQUIRE(model.get_order_arrival_at_exchange_ts(decision_ts) == t0 + 50us);
    }

    SECTION("Ack Arrival at Strategy") {
        market_replay::Timestamp order_arrival_exchange = t0 + 50us; // From previous section result
        market_replay::Timestamp expected_ack_processing_done = order_arrival_exchange + config.exchange_order_processing_latency;
        market_replay::Timestamp expected_ack_arrival_strategy = expected_ack_processing_done + config.ack_network_latency_exch_to_strat;
        
        REQUIRE(model.get_ack_arrival_at_strategy_ts(order_arrival_exchange) == expected_ack_arrival_strategy);
        REQUIRE(model.get_ack_arrival_at_strategy_ts(order_arrival_exchange) == t0 + 50us + 20us + 50us); // 120us total from order_arrival_exchange
    }

    SECTION("Fill Arrival at Strategy") {
        market_replay::Timestamp order_arrival_exchange = t0 + 50us;
        market_replay::Timestamp expected_fill_processing_done = order_arrival_exchange + config.exchange_fill_processing_latency;
        market_replay::Timestamp expected_fill_arrival_strategy = expected_fill_processing_done + config.ack_network_latency_exch_to_strat;

        REQUIRE(model.get_fill_arrival_at_strategy_ts(order_arrival_exchange) == expected_fill_arrival_strategy);
        REQUIRE(model.get_fill_arrival_at_strategy_ts(order_arrival_exchange) == t0 + 50us + 30us + 50us); // 130us total
    }
}