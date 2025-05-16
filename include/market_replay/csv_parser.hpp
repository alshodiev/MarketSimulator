#ifndef MARKET_REPLAY_CSV_PARSER_HPP
#define MARKET_REPLAY_CSV_PARSER_HPP

#include "event.hpp"
#include "logger.hpp"
#include <string>
#include <fstream>
#include <vector>
#include <sstream> // For std::stringstream

namespace market_replay {

class CsvParser {
public:
    CsvParser(const std::string& filepath);
    ~CsvParser();

    // Reads the next event from the CSV. Returns nullptr if EOF or error.
    std::unique_ptr<BaseEvent> read_next_event();
    bool has_more_events() const;

private:
    std::ifstream file_stream_;
    std::string current_line_;
    long long line_number_ = 0;

    // Helper to split CSV line
    std::vector<std::string> split_line(const std::string& line, char delimiter = ',');
};

} // namespace market_replay
#endif // MARKET_REPLAY_CSV_PARSER_HPP