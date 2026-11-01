#pragma once

#include "BoundedQueue.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace X {

// 工作窃取线程池：每个 worker 拥有本地有界队列，空闲时窃取其他 worker 的任务。
// 统一由全局计数控制总容量：构造时只指定总任务数，按线程数尽量平分到各本地队列，
// 提交任务时若任务总数已达全局容量则直接丢弃。
// worker 空闲时限时等待本地队列，超时后重新窃取其他队列；关闭时由 stop 请求唤醒。
class ThreadPool {

public:
    using Task = std::function<void()>;

private:
    // 空闲 worker 限时等待本地队列的时长；超时后回到循环顶部重新窃取。
    // 越小窃取越及时（负载更均衡），越大越省 CPU。
    static constexpr std::chrono::milliseconds IDLE_POLL_TIMEOUT { 10 };

    std::size_t numThreads_;
    std::size_t capacity_; // 全局总容量：所有本地队列中任务数之和的上限
    std::atomic<std::size_t> nextSubmitIndex_ { 0 };
    std::mutex mutex_; // 仅保护 stopping_ 与提交侧预留，以及关闭时的退出检查（非热路径）
    bool stopping_ { false };
    std::atomic<std::size_t> pendingTasks_ { 0 }; // 无锁全局计数：worker 递减无需持锁
    std::stop_source stopSource_; // 关闭信号：唤醒阻塞在 pop 上的 worker
    std::vector<std::unique_ptr<BoundedQueue<Task>>> queues_;
    std::vector<std::jthread> workers_; // 最后声明：最先析构，确保 join 早于 mutex_ 等销毁

public:
    // 构造：线程数 + 全局总任务容量
    explicit ThreadPool(std::size_t numThreads,
        std::size_t totalCapacity)
        : numThreads_(numThreads)
        , capacity_(totalCapacity)
    {
        init_();
    }

    ~ThreadPool()
    {
        stop();
    }

    // 停止接收新任务，并唤醒所有阻塞在 pop 上的 worker（已入队任务执行完毕后退出）
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock { mutex_ };
            stopping_ = true;
        }
        stopSource_.request_stop();
    }

    // 提交任务，返回结果的 future。
    // 若任务总数已达全局容量，任务被直接丢弃：返回的 future 在 get() 时
    // 抛出 std::future_error(broken_promise)（packaged_task 未被运行即销毁）。
    template <typename ReturnType>
    auto submit(std::function<ReturnType()> func)
        -> std::future<ReturnType>
    {
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::move(func));
        auto future = task->get_future();

        // 统一控制：全局计数达容量即丢弃，与 stop() 的 stopping_ 判定在同一把锁下。
        {
            auto _ = std::lock_guard<std::mutex> { mutex_ };
            if (stopping_) {
                throw std::runtime_error("线程池已停止，无法提交新任务");
            }
            if (pendingTasks_.load(std::memory_order_relaxed) >= capacity_) {
                return future; // 丢弃：task 析构时未运行，future 变为 broken_promise
            }
            pendingTasks_.fetch_add(1, std::memory_order_relaxed); // 预留槽位，先于入队
        }

        const auto startIdx = nextSubmitIndex_.fetch_add(1, std::memory_order_relaxed) % numThreads_;

        // 包装成可入队的 std::function<void()>（shared_ptr 无法隐式转换为 std::function）
        auto taskWrapper = [task]() { (*task)(); };

        // round-robin：优先目标队列，满则依次尝试其他队列。
        // tryPush 按值接收（满时不消耗原对象），故此处拷贝入队而非移动。
        // 因全局计数已保证存在空闲槽位，循环内必然能入队成功。
        for (std::size_t k = 0; k < numThreads_; ++k) {
            auto idx = (startIdx + k) % numThreads_;
            if (queues_[idx]->tryPush(std::move(taskWrapper)) == BoundedQueueErrc::SUCCESS) {
                return future;
            }
        }

        // 理论上不可达：防御性兜底，撤销预留并丢弃
        pendingTasks_.fetch_sub(1, std::memory_order_relaxed);
        return future;
    }

    void init_()
    {
        assert(numThreads_ > 0);
        assert(capacity_ > 0);

        // 全局容量按线程数尽量平分：前 capacity_ % numThreads_ 个队列多分 1 个槽位
        const std::size_t base = capacity_ / numThreads_;
        const std::size_t remainder = capacity_ % numThreads_;

        // 每个本地队列共享同一个 stop_token，由 stopSource_ 统一控制关闭
        const auto stopToken = stopSource_.get_token();

        queues_.reserve(numThreads_);
        for (std::size_t i = 0; i < numThreads_; ++i) {
            const std::size_t cap = base + (i < remainder ? 1 : 0);
            queues_.emplace_back(std::make_unique<BoundedQueue<Task>>(cap, stopToken));
        }

        workers_.reserve(numThreads_);
        for (std::size_t i = 0; i < numThreads_; ++i) {
            workers_.emplace_back([this, i] { workLoop_(i); });
        }
    }

private:
    void workLoop_(std::size_t index)
    {
        auto& taskQueue = *queues_[index];
        while (true) {
            Task task;

            // 优先从本地队列取任务
            if (taskQueue.tryPop(task) == BoundedQueueErrc::SUCCESS) {
                pendingTasks_.fetch_sub(1, std::memory_order_relaxed);
                task();
                continue;
            }

            // 本地为空：限时等待本地队列。超时后再窃取，避免每个任务后都全量扫描其他队列
            // 造成队列锁竞争（这正是线程数增多后吞吐崩塌的原因）。
            auto rc = taskQueue.tryPop(task, IDLE_POLL_TIMEOUT);
            if (rc == BoundedQueueErrc::SUCCESS) {
                pendingTasks_.fetch_sub(1, std::memory_order_relaxed);
                task();
                continue;
            }

            // TIMEOUT 或 STOPPED
            if (rc == BoundedQueueErrc::STOPPED) {
                // 关闭且本地队列空。若仍有残留任务（含已预留未入队），
                // 则继续窃取，否则退出。
                auto _ = std::lock_guard<std::mutex> { mutex_ };
                if (pendingTasks_.load(std::memory_order_relaxed) == 0) {
                    return;
                }
            }

            // 本地空闲超时（或停止但仍有残留）：窃取其他 worker 的队列
            for (std::size_t k = 1; k < numThreads_; ++k) {

                auto& otherTaskQueue = *queues_[(index + k) % numThreads_];
                if (otherTaskQueue.tryPop(task) == BoundedQueueErrc::SUCCESS) {
                    pendingTasks_.fetch_sub(1, std::memory_order_relaxed);
                    task();
                    break; // 窃取到一个任务，处理完回到循环顶部
                }
            }
            // 未窃取到：继续循环，再次限时等待
        }
    }
};

} // namespace X
