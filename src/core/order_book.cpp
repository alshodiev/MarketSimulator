#include "market_replay/order_book.hpp"
#include "market_replay/logger.hpp"
#include <cmath> // For std::isnan

namespace market_replay {

void SimpleOrderBook::update_quote(const QuoteEvent& quote) {
    if (quote.symbol != symbol_) return;

    if (quote.bid_price > 0 && quote.bid_size > 0) {
        best_bid_price_ = quote.bid_price;
        best_bid_size_ = quote.bid_size;
    } else { // Clear side if price/size is invalid
        best_bid_price_ = std::nullopt;
        best_bid_size_ = std::nullopt;
    }

    if (quote.ask_price > 0 && quote.ask_size > 0) {
        best_ask_price_ = quote.ask_price;
        best_ask_size_ = quote.ask_size;
    } else {
        best_ask_price_ = std::nullopt;
        best_ask_size_ = std::nullopt;
    }
    // LOG_DEBUG("OrderBook [{}]: Updated. Bid: {}@{}, Ask: {}@{}", symbol_,
    //     best_bid_price_.value_or(0.0), best_bid_size_.value_or(0),
    //     best_ask_price_.value_or(0.0), best_ask_size_.value_or(0));
}

std::pair<Price, Quantity> SimpleOrderBook::match_market_order(OrderSide side, Quantity quantity) {
    if (quantity == 0) return {INVALID_PRICE, 0};

    Price fill_price = INVALID_PRICE;
    Quantity filled_quantity = 0;

    if (side == OrderSide::BUY) {
        if (best_ask_price_ && best_ask_size_ && *best_ask_size_ > 0) {
            fill_price = *best_ask_price_;
            filled_quantity = std::min(quantity, *best_ask_size_);
            *best_ask_size_ -= filled_quantity;
            if (*best_ask_size_ == 0) { // Liquidity consumed
                best_ask_price_ = std::nullopt;
            }
        } else {
            LOG_WARN("OrderBook [{}]: Cannot match BUY market order, no ask liquidity.", symbol_);
        }
    } else { // SELL
        if (best_bid_price_ && best_bid_size_ && *best_bid_size_ > 0) {
            fill_price = *best_bid_price_;
            filled_quantity = std::min(quantity, *best_bid_size_);
            *best_bid_size_ -= filled_quantity;
            if (*best_bid_size_ == 0) { // Liquidity consumed
                best_bid_price_ = std::nullopt;
            }
        } else {
            LOG_WARN("OrderBook [{}]: Cannot match SELL market order, no bid liquidity.", symbol_);
        }
    }
    if (filled_quantity > 0) {
         LOG_DEBUG("OrderBook [{}]: Matched MKT {} {} @ {}. New AskSz: {}, New BidSz: {}", symbol_,
            (side == OrderSide::BUY ? "BUY" : "SELL"), filled_quantity, fill_price,
            best_ask_size_.value_or(0), best_bid_size_.value_or(0));
    }
    return {fill_price, filled_quantity};
}


std::pair<Price, Quantity> SimpleOrderBook::match_limit_order(OrderSide side, Price limit_price, Quantity quantity) {
    if (quantity == 0 || std::isnan(limit_price)) return {INVALID_PRICE, 0};

    Price fill_price = INVALID_PRICE;
    Quantity filled_quantity = 0;

    if (side == OrderSide::BUY) {
        // Buy limit order is aggressive if limit_price >= best_ask_price
        if (best_ask_price_ && *best_ask_price_ > 0 && limit_price >= (*best_ask_price_ - PRICE_EPSILON)) {
            fill_price = *best_ask_price_; // Filled at ask
            filled_quantity = std::min(quantity, *best_ask_size_);
            *best_ask_size_ -= filled_quantity;
             if (*best_ask_size_ == 0) {
                best_ask_price_ = std::nullopt;
            }
        } else {
            // Passive or unfillable at current market
            LOG_DEBUG("OrderBook [{}]: BUY limit order {} for {} @ {} is passive or unfillable.", symbol_, quantity, limit_price, best_ask_price_.value_or(0.0));
        }
    } else { // SELL
        // Sell limit order is aggressive if limit_price <= best_bid_price
        if (best_bid_price_ && *best_bid_size_ > 0 && limit_price <= (*best_bid_price_ + PRICE_EPSILON)) {
            fill_price = *best_bid_price_; // Filled at bid
            filled_quantity = std::min(quantity, *best_bid_size_);
            *best_bid_size_ -= filled_quantity;
            if (*best_bid_size_ == 0) {
                best_bid_price_ = std::nullopt;
            }
        } else {
            // Passive or unfillable
            LOG_DEBUG("OrderBook [{}]: SELL limit order {} for {} @ {} is passive or unfillable.", symbol_, quantity, limit_price, best_bid_price_.value_or(0.0));
        }
    }
    if (filled_quantity > 0) {
         LOG_DEBUG("OrderBook [{}]: Matched LMT {} {} @ {} (Limit {}). New AskSz: {}, New BidSz: {}", symbol_,
            (side == OrderSide::BUY ? "BUY" : "SELL"), filled_quantity, fill_price, limit_price,
            best_ask_size_.value_or(0), best_bid_size_.value_or(0));
    }
    return {fill_price, filled_quantity};
}


} // namespace market_replay