#ifndef MARKET_REPLAY_ORDER_BOOK_HPP
#define MARKET_REPLAY_ORDER_BOOK_HPP

#include "common.hpp"
#include "event.hpp" // For QuoteEvent
#include <map>
#include <string>
#include <optional>

namespace market_replay {

// A very simplified representation of an order book for a single symbol
// Mainly to get current BBO for matching market orders.
class SimpleOrderBook {
public:
    SimpleOrderBook(std::string symbol) : symbol_(std::move(symbol)) {}

    void update_quote(const QuoteEvent& quote);

    std::optional<Price> get_bid_price() const { return best_bid_price_; }
    std::optional<Quantity> get_bid_size() const { return best_bid_size_; }
    std::optional<Price> get_ask_price() const { return best_ask_price_; }
    std::optional<Quantity> get_ask_size() const { return best_ask_size_; }

    const std::string& get_symbol() const { return symbol_; }

    // Tries to match a market order, returns fill price and quantity.
    // Reduces liquidity from the book.
    std::pair<Price, Quantity> match_market_order(OrderSide side, Quantity quantity);

    // Tries to match a limit order. If aggressive, behaves like market.
    // If passive, it would be added to a real book. Here, we just check immediate fill.
    // Returns fill price and quantity.
    std::pair<Price, Quantity> match_limit_order(OrderSide side, Price limit_price, Quantity quantity);


private:
    std::string symbol_;
    std::optional<Price> best_bid_price_;
    std::optional<Quantity> best_bid_size_;
    std::optional<Price> best_ask_price_;
    std::optional<Quantity> best_ask_size_;
    Timestamp last_trade_price_ts_;
    Price last_trade_price_ = INVALID_PRICE;


    // For a more complex book:
    // std::map<Price, Quantity, std::greater<Price>> bids_; // Price -> Total Quantity
    // std::map<Price, Quantity, std::less<Price>> asks_;    // Price -> Total Quantity
};

} // namespace market_replay
#endif // MARKET_REPLAY_ORDER_BOOK_HPP