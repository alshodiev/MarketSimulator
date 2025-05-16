#ifndef MARKET_REPLAY_LOGGER_HPP
#define MARKET_REPLAY_LOGGER_HPP

#include <iostream>
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <string>
#include <memory> // For std::shared_ptr

namespace market_replay {
class Logger {
public:
    // Call once at the beginning of main()
    static void init(const std::string& log_file_name = "replay_log.txt", 
                     spdlog::level::level_enum console_level = spdlog::level::info,
                     spdlog::level::level_enum file_level = spdlog::level::debug,
                     bool async = true,
                     size_t async_q_size = 8192,
                     size_t async_threads = 1) {
        
        if (Logger::is_initialized_) {
            spdlog::warn("Logger already initialized.");
            return;
        }

        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(console_level);
            // Format: [Timestamp] [Logger Name] [Thread ID] [Level] Message
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%t] [%^%l%$] %v");


            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_name, true); // true to truncate
            file_sink->set_level(file_level);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%t] [%l] %v");

            std::shared_ptr<spdlog::logger> logger;
            if (async) {
                spdlog::init_thread_pool(async_q_size, async_threads);
                logger = std::make_shared<spdlog::async_logger>("MarketReplaySim",
                                                                spdlog::sinks_init_list{console_sink, file_sink},
                                                                spdlog::thread_pool(),
                                                                spdlog::async_overflow_policy::block);
            } else {
                 logger = std::make_shared<spdlog::logger>("MarketReplaySim",
                                                          spdlog::sinks_init_list{console_sink, file_sink});
            }
            
            spdlog::set_default_logger(logger);
            spdlog::set_level(std::min(console_level, file_level)); // Global level set to the more verbose of the two
            spdlog::flush_on(spdlog::level::info); 
            
            Logger::is_initialized_ = true;
            spdlog::info("Logger initialized. Console Level: {}, File Level: {}, Async: {}", 
                spdlog::level::to_string_view(console_level), 
                spdlog::level::to_string_view(file_level),
                async);

        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
            Logger::is_initialized_ = false; // Explicitly set on failure
        }
    }

    static void shutdown() {
        if (Logger::is_initialized_) {
            spdlog::info("Logger shutting down.");
            spdlog::shutdown();
            Logger::is_initialized_ = false;
        }
    }
    
    static bool is_initialized() { return Logger::is_initialized_; }

private:
    // To prevent multiple initializations if init() is called more than once.
    inline static bool is_initialized_ = false; 
};
} // namespace market_replay

// Global macros for convenience
#define LOG_TRACE(...)    if(market_replay::Logger::is_initialized()) spdlog::trace(__VA_ARGS__); else std::cout << "LOG_TRACE: " << fmt::format(__VA_ARGS__) << std::endl
#define LOG_DEBUG(...)    if(market_replay::Logger::is_initialized()) spdlog::debug(__VA_ARGS__); else std::cout << "LOG_DEBUG: " << fmt::format(__VA_ARGS__) << std::endl
#define LOG_INFO(...)     if(market_replay::Logger::is_initialized()) spdlog::info(__VA_ARGS__); else std::cout << "LOG_INFO: " << fmt::format(__VA_ARGS__) << std::endl
#define LOG_WARN(...)     if(market_replay::Logger::is_initialized()) spdlog::warn(__VA_ARGS__); else std::cerr << "LOG_WARN: " << fmt::format(__VA_ARGS__) << std::endl
#define LOG_ERROR(...)    if(market_replay::Logger::is_initialized()) spdlog::error(__VA_ARGS__); else std::cerr << "LOG_ERROR: " << fmt::format(__VA_ARGS__) << std::endl
#define LOG_CRITICAL(...) if(market_replay::Logger::is_initialized()) spdlog::critical(__VA_ARGS__); else std::cerr << "LOG_CRITICAL: " << fmt::format(__VA_ARGS__) << std::endl

#endif // MARKET_REPLAY_LOGGER_HPP