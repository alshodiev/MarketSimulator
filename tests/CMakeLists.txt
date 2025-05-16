cmake_minimum_required(VERSION 3.16)
project(MarketReplaySimulatorTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include main project's include directory
include_directories(${CMAKE_SOURCE_DIR}/include)

# Test sources
set(TEST_SOURCES
    test_main.cpp
    test_common.cpp
    test_blocking_queue.cpp
    test_latency_model.cpp
    test_csv_parser.cpp
    # test_dispatcher.cpp # Add when ready, more complex
    # mock_strategy.hpp # Not a source, but needed for some tests
)

add_executable(run_tests ${TEST_SOURCES})

target_link_libraries(run_tests 
    PRIVATE 
    Catch2::Catch2WithMain 
    market_replay_core # Link against the core library from parent project
    Threads::Threads   # If tests use threading directly or indirectly via core
    spdlog::spdlog     # If tests use logging
)

# Add test to CTest
include(CTest)
add_test(NAME FullTestSuite COMMAND run_tests)