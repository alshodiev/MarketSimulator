#include "market_replay/csv_parser.hpp"
#include "market_replay/common.hpp" // For string_to_timestamp, Price, Quantity
#include <iostream> // For std::cerr in case of critical error before logger

namespace market_replay {

CsvParser::CsvParser(const std::string& filepath) : line_number_(0) {
    file_stream_.open(filepath);
    if (!file_stream_.is_open()) {
        LOG_CRITICAL("Failed to open CSV file: {}", filepath);
        throw std::runtime_error("Failed to open CSV file: " + filepath);
    }
    // Skip header line
    if (std::getline(file_stream_, current_line_)) {
        line_number_++;
        LOG_INFO("CSV Parser: Skipped header line: {}", current_line_);
    } else {
        LOG_WARN("CSV Parser: File is empty or contains no header: {}", filepath);
    }
}

CsvParser::~CsvParser() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

bool CsvParser::has_more_events() const {
    return file_stream_.good() && !file_stream_.eof();
}

std::vector<std::string> CsvParser::split_line(const std::string& line, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(line);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::unique_ptr<BaseEvent> CsvParser::read_next_event() {
    if (!std::getline(file_stream_, current_line_)) {
        return nullptr; // EOF or error
    }
    line_number_++;

    auto tokens = split_line(current_line_);
    if (tokens.empty()) {
        LOG_WARN("CSV Parser: Empty line at {}", line_number_);
        return nullptr; 
    }

    try {
        const std::string& type_str = tokens[0];
        Timestamp exchange_ts = string_to_timestamp(tokens[1]);
        const std::string& symbol = tokens[2];

        if (type_str == "QUOTE" && tokens.size() >= 9) {
            // TYPE,TIMESTAMP_NS,SYMBOL,PRICE(unused),SIZE(unused),BID_PRICE,BID_SIZE,ASK_PRICE,ASK_SIZE
            Price bid_price = std::stod(tokens[5]);
            Quantity bid_size = std::stoull(tokens[6]);
            Price ask_price = std::stod(tokens[7]);
            Quantity ask_size = std::stoull(tokens[8]);
            return std::make_unique<QuoteEvent>(exchange_ts, symbol, bid_price, bid_size, ask_price, ask_size);
        } else if (type_str == "TRADE" && tokens.size() >= 5) {
            // TYPE,TIMESTAMP_NS,SYMBOL,PRICE,SIZE
            Price price = std::stod(tokens[3]);
            Quantity size = std::stoull(tokens[4]);
            return std::make_unique<TradeEvent>(exchange_ts, symbol, price, size);
        } else {
            LOG_WARN("CSV Parser: Unknown event type '{}' or malformed line {} : {}", type_str, line_number_, current_line_);
            return nullptr;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("CSV Parser: Error parsing line {}: {} - Error: {}", line_number_, current_line_, e.what());
        return nullptr;
    }
}
} // namespace market_replay