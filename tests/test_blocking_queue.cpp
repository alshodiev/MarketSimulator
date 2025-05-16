#include "catch2/catch_test_macros.hpp"
#include "market_replay/utils/blocking_queue.hpp"
#include <thread>
#include <vector>
#include <numeric> // For std::iota

TEST_CASE("BlockingQueue Basic Operations", "[blocking_queue]") {
    market_replay::utils::BlockingQueue<int> queue;

    SECTION("Push and Pop") {
        queue.push(10);
        int val;
        REQUIRE(queue.wait_and_pop(val));
        REQUIRE(val == 10);
        REQUIRE(queue.empty());
    }

    SECTION("Try Pop") {
        REQUIRE_FALSE(queue.try_pop().has_value());
        queue.push(20);
        auto opt_val = queue.try_pop();
        REQUIRE(opt_val.has_value());
        REQUIRE(opt_val.value() == 20);
        REQUIRE_FALSE(queue.try_pop().has_value());
    }

    SECTION("Shutdown empty queue") {
        queue.shutdown();
        int val;
        REQUIRE_FALSE(queue.wait_and_pop(val)); // Should return false on shutdown
    }

    SECTION("Shutdown non-empty queue") {
        queue.push(30);
        queue.shutdown();
        int val;
        REQUIRE(queue.wait_and_pop(val)); // Should pop remaining items
        REQUIRE(val == 30);
        REQUIRE_FALSE(queue.wait_and_pop(val)); // Then return false
    }
}

TEST_CASE("BlockingQueue Multithreaded", "[blocking_queue]") {
    market_replay::utils::BlockingQueue<int> queue(5); // Bounded queue
    const int num_items = 100;

    SECTION("Single Producer, Single Consumer") {
        std::thread producer([&]() {
            for (int i = 0; i < num_items; ++i) {
                queue.push(i);
            }
        });

        std::thread consumer([&]() {
            for (int i = 0; i < num_items; ++i) {
                int val;
                REQUIRE(queue.wait_and_pop(val));
                REQUIRE(val == i);
            }
        });

        producer.join();
        consumer.join();
        REQUIRE(queue.empty());
    }

    SECTION("Multiple Producers, Single Consumer") {
        const int num_producers = 4;
        const int items_per_producer = num_items / num_producers;
        std::vector<std::thread> producers;

        for (int p = 0; p < num_producers; ++p) {
            producers.emplace_back([&, p]() {
                for (int i = 0; i < items_per_producer; ++i) {
                    queue.push(p * items_per_producer + i);
                }
            });
        }

        std::vector<int> consumed_items;
        std::thread consumer([&]() {
            for (int i = 0; i < num_items; ++i) {
                int val;
                if (queue.wait_and_pop(val)) {
                    consumed_items.push_back(val);
                } else {
                    break; // Shutdown or error
                }
            }
        });

        for (auto& t : producers) {
            t.join();
        }
        
        // Wait for consumer to finish or use timed pop with shutdown
        // For this test, we know all items are pushed, consumer should get them.
        // A robust way is to signal consumer when all producers are done.
        // Or consumer can use timed_wait_and_pop until queue is empty AND producers finished.
        // Here, we just wait a bit then shutdown.
        
        // To ensure consumer finishes processing items placed before shutdown:
        while(queue.size() > 0) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
        queue.shutdown(); // Signal consumer to stop if it's waiting

        consumer.join();
        
        REQUIRE(consumed_items.size() == num_items);
        std::sort(consumed_items.begin(), consumed_items.end());
        std::vector<int> expected_items(num_items);
        std::iota(expected_items.begin(), expected_items.end(), 0);
        REQUIRE(consumed_items == expected_items);
    }

    SECTION("Timed Wait and Pop") {
        int val;
        REQUIRE_FALSE(queue.timed_wait_and_pop(val, std::chrono::milliseconds(10))); // Timeout
        queue.push(123);
        REQUIRE(queue.timed_wait_and_pop(val, std::chrono::milliseconds(10)));
        REQUIRE(val == 123);
    }
}