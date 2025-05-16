#ifndef MARKET_REPLAY_BLOCKING_QUEUE_HPP
#define MARKET_REPLAY_BLOCKING_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <optional>
#include <chrono> // For timeout

namespace market_replay {
namespace utils {

template <typename T>
class BlockingQueue {
public:
    BlockingQueue(size_t max_size = 0) : max_size_(max_size) {} // 0 means unbounded

    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;
    BlockingQueue(BlockingQueue&&) = default; 
    BlockingQueue& operator=(BlockingQueue&&) = default;

    void push(T item) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (max_size_ > 0) {
                cv_producer_.wait(lock, [this] { return queue_.size() < max_size_ || shutdown_requested_; });
            }
            if (shutdown_requested_) return; // Don't push if shutting down
            
            queue_.push(std::move(item));
        }
        cv_consumer_.notify_one();
    }

    bool wait_and_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_consumer_.wait(lock, [this] { return !queue_.empty() || shutdown_requested_; });
        
        if (shutdown_requested_ && queue_.empty()) {
            return false; 
        }
        if (!queue_.empty()) {
            item = std::move(queue_.front());
            queue_.pop();
            cv_producer_.notify_one(); // Notify producer if there's space
            return true;
        }
        return false; // Should be covered by shutdown_requested_ check
    }
    
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty() || shutdown_requested_) { // Check shutdown_requested_ here too
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        cv_producer_.notify_one();
        return item;
    }

    // Pop with timeout
    bool timed_wait_and_pop(T& item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_consumer_.wait_for(lock, timeout, [this] { return !queue_.empty() || shutdown_requested_; })) {
            return false; // Timeout
        }
        
        if (shutdown_requested_ && queue_.empty()) {
            return false;
        }
        if (!queue_.empty()) {
            item = std::move(queue_.front());
            queue_.pop();
            cv_producer_.notify_one();
            return true;
        }
        return false;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_requested_ = true;
        }
        cv_consumer_.notify_all();
        cv_producer_.notify_all();
    }

    bool is_shutdown() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutdown_requested_;
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_consumer_; // Signaled when item is pushed
    std::condition_variable cv_producer_; // Signaled when item is popped (if bounded)
    bool shutdown_requested_ = false;
    size_t max_size_ = 0; // 0 for unbounded
};

} // namespace utils
} // namespace market_replay
#endif // MARKET_REPLAY_BLOCKING_QUEUE_HPP