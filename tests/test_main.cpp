// This file can be empty if Catch2::Catch2WithMain is used, as it provides main.
// Or, define your own main for custom setup.
// #define CATCH_CONFIG_MAIN // Define this in only one .cpp file if not using Catch2WithMain link target
// #include "catch2/catch.hpp"

// If using Catch2WithMain, this file is primarily for any global setup/teardown for tests,
// or can be omitted if not needed.
#define CATCH_CONFIG_MAIN
#include "catch2/catch_test_macros.hpp"
#include "market_replay/common.hpp"
#include "market_replay/logger.hpp" // For initializing logger if tests use it
#include <string>
#include <ostream>


struct GlobalTestInitializer {
    GlobalTestInitializer() {
        // Initialize logger for tests, but maybe to console only or a test-specific file
        // market_replay::Logger::init("test_run_log.txt", spdlog::level::debug, spdlog::level::trace, false);
    }
    ~GlobalTestInitializer() {
        // market_replay::Logger::shutdown();
    }
};
// GlobalTestInitializer g_test_initializer; // Create an instance to run constructor/destructor
