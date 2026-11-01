#pragma once
#include "Singleton.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include <system_error>
#include <type_traits>

namespace X {

enum class BoundedQueueErrc : int {
    SUCCESS = 0,
    STOPPED = 10,
    TIMEOUT = 11,
    FULL = 12,
    EMPTY = 13,
};

class BoundedQueueErrCatogory
    : public std::error_category,
      public SingletonWithConstructToken<BoundedQueueErrCatogory> {

public:
    explicit BoundedQueueErrCatogory(ConstructToken /*unused*/) { }

    [[nodiscard]] auto name() const noexcept
        -> const char* override
    {
        return "BoundedQueueErrCatogory";
    }

    [[nodiscard]] auto message(int ec) const
        -> std::string override
    {
        switch (static_cast<BoundedQueueErrc>(ec)) {
        case BoundedQueueErrc::SUCCESS:
            return "成功";
        case BoundedQueueErrc::STOPPED:
            return "已停止";
        case BoundedQueueErrc::TIMEOUT:
            return "操作超时!";
        case BoundedQueueErrc::FULL:
            return "队列已满";
        case BoundedQueueErrc::EMPTY:
            return "队列为空";
        default:
            return "未知错误";
        }
    }

    // 把自定义错误号映射到通用 error_condition,实现跨 category 等价
    [[nodiscard]] auto default_error_condition(int ec) const noexcept
        -> std::error_condition override
    {
        switch (static_cast<BoundedQueueErrc>(ec)) {
        case BoundedQueueErrc::TIMEOUT:
            return std::make_error_condition(std::errc::timed_out);

        default:
            return { ec, *this }; // 其他用本 category 自身
        }
    }
};

template <typename T>
class BoundedQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable_any notFullCond_;
    std::condition_variable_any notEmptyCond_;
    std::size_t capacity_;
    std::stop_token stopToken_; // 构造时传入，用于关闭控制

public:
    explicit BoundedQueue(size_t capacity, std::stop_token st = {})
        : capacity_(capacity)
        , stopToken_(std::move(st))
    {
    }

    // 阻塞入队：队列满时等待，直到有空间或 stop 请求（此时返回 STOPPED）
    auto push(T item)
        -> BoundedQueueErrc
    {
        auto uLock = std::unique_lock<std::mutex> { mutex_ };
        bool hasSpace = notFullCond_.wait(uLock, stopToken_, [this]() { return queue_.size() < capacity_; });
        if (!hasSpace) {
            return BoundedQueueErrc::STOPPED;
        }

        queue_.push(std::move(item));
        notEmptyCond_.notify_one();
        return BoundedQueueErrc::SUCCESS;
    }

    // 阻塞出队：队列空时等待，直到有元素或 stop 请求（此时返回 STOPPED）
    auto pop(T& item)
        -> BoundedQueueErrc
    {
        auto uLock = std::unique_lock<std::mutex> { mutex_ };
        bool hasItem = notEmptyCond_.wait(uLock, stopToken_, [this]() { return !queue_.empty(); });
        if (!hasItem) {
            return BoundedQueueErrc::STOPPED;
        }

        item = std::move(queue_.front());
        queue_.pop();
        notFullCond_.notify_one();
        return BoundedQueueErrc::SUCCESS;
    }

    // 限时入队：队列满时等待，超时返回 TIMEOUT，stop 请求返回 STOPPED
    template <typename Rep, typename Period>
    auto tryPush(T item, const std::chrono::duration<Rep, Period>& timeout)
        -> BoundedQueueErrc
    {
        auto uLock = std::unique_lock<std::mutex> { mutex_ };
        auto ok = notFullCond_.wait_for(uLock, stopToken_, timeout, [this]() { return queue_.size() < capacity_; });
        if (!ok) {
            return stopToken_.stop_requested() ? BoundedQueueErrc::STOPPED : BoundedQueueErrc::TIMEOUT;
        }

        queue_.push(std::move(item));
        notEmptyCond_.notify_one();
        return BoundedQueueErrc::SUCCESS;
    }

    // 限时出队：队列空时等待，超时返回 TIMEOUT，stop 请求返回 STOPPED
    template <typename Rep, typename Period>
    auto tryPop(T& item, const std::chrono::duration<Rep, Period>& timeout)
        -> BoundedQueueErrc
    {
        auto uLock = std::unique_lock<std::mutex> { mutex_ };
        auto ok = notEmptyCond_.wait_for(uLock, stopToken_, timeout, [this]() { return !queue_.empty(); });
        if (!ok) {
            return stopToken_.stop_requested() ? BoundedQueueErrc::STOPPED : BoundedQueueErrc::TIMEOUT;
        }

        item = std::move(queue_.front());
        queue_.pop();
        notFullCond_.notify_one();
        return BoundedQueueErrc::SUCCESS;
    }

    // 非阻塞入队：队列满时立即返回 FULL，不等待
    auto tryPush(T item)
        -> BoundedQueueErrc
    {
        auto uLock = std::unique_lock<std::mutex> { mutex_ };
        if (queue_.size() >= capacity_) {
            return BoundedQueueErrc::FULL;
        }
        queue_.push(std::move(item));
        notEmptyCond_.notify_one();
        return BoundedQueueErrc::SUCCESS;
    }

    // 非阻塞出队：队列为空时立即返回 EMPTY，不等待
    auto tryPop(T& item)
        -> BoundedQueueErrc
    {
        auto uLock = std::unique_lock<std::mutex> { mutex_ };
        if (queue_.empty()) {
            return BoundedQueueErrc::EMPTY;
        }
        item = std::move(queue_.front());
        queue_.pop();
        notFullCond_.notify_one();
        return BoundedQueueErrc::SUCCESS;
    }

    auto size() const
        -> std::size_t
    {
        auto _ = std::lock_guard<std::mutex> { mutex_ };
        return queue_.size();
    }

    auto empty() const
        -> bool
    {
        auto _ = std::lock_guard<std::mutex> { mutex_ };
        return queue_.empty();
    }

    auto full() const
        -> bool
    {
        auto _ = std::lock_guard<std::mutex> { mutex_ };
        return queue_.size() >= capacity_;
    }

    auto capacity() const
        -> std::size_t
    {
        return capacity_;
    }
};
} // namespace X

namespace std {
template <>
struct is_error_code_enum<X::BoundedQueueErrc>
    : true_type { };
} // namespace std

namespace X {
// 定义在枚举所在命名空间，供 std::error_code 构造时通过 ADL 找到
inline auto make_error_code(BoundedQueueErrc e)
    -> std::error_code
{
    return { static_cast<int>(e), BoundedQueueErrCatogory::getInstanceRef() };
}
} // namespace X
