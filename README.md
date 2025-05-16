# MarketSimulator

![Market Replay Simulator](/assets/thumbnail.png)

## Overview
This project simulates market replay of trading strategies using historical tick data in realistic, low-latency conditions. This simulator allows the user to test strategies under different market regimes and stress, latency delays, and concurrency — without needing access to high-scale production systems.

## Project Structure
```
market_replay_simulator/
├── CMakeLists.txt
├── data/
│   └── sample_ticks.csv
├── include/
│   ├── market_replay/
│   │   ├── common.hpp
│   │   ├── event.hpp
│   │   ├── strategy.hpp
│   │   ├── latency_model.hpp
│   │   ├── logger.hpp
│   │   ├── metrics.hpp
│   │   ├── interfaces.hpp
│   │   ├── dispatcher.hpp
│   │   ├── csv_parser.hpp
│   │   └── utils/
│   │       └── blocking_queue.hpp
├── src/
│   ├── core/
│   │   ├── dispatcher.cpp
│   │   ├── latency_model.cpp
│   │   └── order_book.cpp  // Simple order book for matching
│   │   └── order_book.hpp
│   ├── io/
│   │   └── csv_parser.cpp
│   ├── strategy/
│   │   └── basic_strategy.cpp
│   ├── utils/
│   │   ├── logger.cpp
│   │   ├── metrics.cpp
│   │   └── common.cpp
│   └── main.cpp
├── strategies/
│   └── (empty for now, user strategies would go here or be linked)
└── tests/
    ├── CMakeLists.txt
    ├── test_main.cpp
    ├── test_common.cpp
    ├── test_blocking_queue.cpp
    ├── test_latency_model.cpp
    ├── test_csv_parser.cpp
    └── test_dispatcher.cpp
    └── mock_strategy.hpp
```

## Features
- Replay historical tick-level data
- Simulate simple order submission and acknowledgment
- Run execution strategies via plug-in hooks
- Log metrics in real-time 
- Test functionalities using Unit Tests

## Outputs
- Simulated trades, PnL, and timing metrics
- Logs of strategy actions (order submits, fills)
- Latency breakdown (event dispatch, strategy reaction, order ack)

## Future improvements
- Add actual strategies that produce buy/sell/hold signals
- Productionize and improve performance for multi-node setup

