# MarketSimulator
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

[![Demo](https://github.com/alshodiev/MarketSimulator/raw/refs/heads/main/assets/market_replay.mov)](https://github.com/alshodiev/MarketSimulator/raw/refs/heads/main/assets/market_replay.mov)

