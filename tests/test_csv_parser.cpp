#include "catch2/catch_test_macros.hpp"
#include "test_utils.hpp"
#include "market_replay/csv_parser.hpp"
#include "market_replay/event.hpp"
#include <fstream> 

void create_dummy_csv(const std::string& filename, const std::vector<std::string>& lines) {
    std::ofstream file(filename);
    for (const auto& line : lines) {
        file << line << std::endl;
    }
    file.close();
}

TEST_CASE("CsvParser Functionality", "[csv_parser]") {
    const std::string test_csv_file = "test_data.csv";
    market_replay::Logger::init("test_parser_log.txt", spdlog::level::off, spdlog::level::off, false); // Suppress logs for this test

    SECTION("Valid CSV data") {
        create_dummy_csv(test_csv_file, {
            "TYPE,TIMESTAMP_NS,SYMBOL,PRICE,SIZE,BID_PRICE,BID_SIZE,ASK_PRICE,ASK_SIZE",
            "QUOTE,1678886400000000000,EURUSD,0,0,1.07100,100000,1.07105,100000",
            "TRADE,1678886400000500000,EURUSD,1.07105,10000,,,"
        });

        market_replay::CsvParser parser(test_csv_file);
        REQUIRE(parser.has_more_events());

        auto event1 = parser.read_next_event();
        REQUIRE(event1 != nullptr);
        REQUIRE(event1->type == market_replay::EventType::QUOTE);
        auto quote_event = dynamic_cast<market_replay::QuoteEvent*>(event1.get());
        REQUIRE(quote_event != nullptr);
        REQUIRE(quote_event->symbol == "EURUSD");
        REQUIRE(quote_event->bid_price == 1.07100);
        REQUIRE(quote_event->ask_size == 100000);
        REQUIRE(quote_event->exchange_timestamp == market_replay::string_to_timestamp("1678886400000000000"));

        auto event2 = parser.read_next_event();
        REQUIRE(event2 != nullptr);
        REQUIRE(event2->type == market_replay::EventType::TRADE);
        auto trade_event = dynamic_cast<market_replay::TradeEvent*>(event2.get());
        REQUIRE(trade_event != nullptr);
        REQUIRE(trade_event->symbol == "EURUSD");
        REQUIRE(trade_event->price == 1.07105);
        REQUIRE(trade_event->size == 10000);
        
        REQUIRE_FALSE(parser.read_next_event()); // EOF
        REQUIRE_FALSE(parser.has_more_events());
    }
    
    SECTION("Malformed CSV line") {
        create_dummy_csv(test_csv_file, {
            "TYPE,TIMESTAMP_NS,SYMBOL,PRICE,SIZE,BID_PRICE,BID_SIZE,ASK_PRICE,ASK_SIZE",
            "QUOTE,bad_timestamp,EURUSD,0,0,1.07100,100000,1.07105,100000"
        });
        market_replay::CsvParser parser(test_csv_file);
        auto event = parser.read_next_event(); // Should log error and return nullptr
        REQUIRE(event == nullptr); 
    }
    
    SECTION("Non-existent file") {
    REQUIRE_THROWS_AS([&]() { market_replay::CsvParser("non_existent_file.csv"); }(), std::runtime_error);
}
    market_replay::Logger::shutdown();
    std::remove(test_csv_file.c_str()); // Clean up
}