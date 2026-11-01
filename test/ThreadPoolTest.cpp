// ThreadPool 线程池测试
//
// 覆盖：全局容量统一控制（任务总数达到容量后直接丢弃）、round-robin 分发、
// 工作窃取（空闲 worker 限时等待后重新窃取）、并发处理与关闭排空。

#include <catch2/catch_test_macros.hpp>

#include "ThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <vector>

namespace {

// 阻塞探针：任务先发出“已进入”信号，再阻塞等待放行。
// 用它占住某个 worker，从而让后续提交的任务稳定积压在队列里。
class BlockingProbe {
public:
    auto makeTask()
    {
        return [this] {
            entered_.set_value();
            release_.get_future().wait();
        };
    }

    void waitEntered() { entered_.get_future().wait(); }
    void unblock() { release_.set_value(); }

private:
    std::promise<void> entered_;
    std::promise<void> release_;
};

} // namespace

TEST_CASE("submit 正常执行并返回结果", "[common][threadpool]")
{
    X::ThreadPool pool(2, 4);
    auto f = pool.submit<int>([] { return 5; });
    CHECK(f.get() == 5);
}

TEST_CASE("submit 支持返回 void 的任务", "[common][threadpool]")
{
    X::ThreadPool pool(1, 4);
    std::atomic<int> value { 0 };
    auto f = pool.submit<void>([&value] { value.fetch_add(1); });
    f.get();
    CHECK(value.load() == 1);
}

TEST_CASE("达到全局容量后直接丢弃新任务", "[common][threadpool]")
{
    BlockingProbe probe;
    X::ThreadPool pool(1, 1); // 1 worker，全局容量 1

    auto blocked = pool.submit<void>(probe.makeTask()); // 占住唯一 worker
    probe.waitEntered();

    auto a = pool.submit<int>([] { return 1; }); // 占用唯一槽位

    std::atomic<int> ran { 0 };
    auto discarded = pool.submit<void>([&ran] { ran.fetch_add(1); }); // 超容量，丢弃

    CHECK_THROWS_AS(discarded.get(), std::future_error); // 丢弃 → broken_promise

    probe.unblock();
    blocked.get();
    CHECK(a.get() == 1);
    CHECK(ran.load() == 0); // 被丢弃的任务未执行
}

TEST_CASE("多个队列共享同一个全局计数", "[common][threadpool]")
{
    X::ThreadPool pool(2, 3); // 2 worker，全局容量 3（1+2 平分）

    BlockingProbe p0;
    BlockingProbe p1;
    auto blocked0 = pool.submit<void>(p0.makeTask()); // → worker 0
    p0.waitEntered();
    auto blocked1 = pool.submit<void>(p1.makeTask()); // → worker 1
    p1.waitEntered();

    auto a = pool.submit<int>([] { return 1; }); // → worker 0 队列
    auto b = pool.submit<int>([] { return 2; }); // → worker 1 队列
    auto c = pool.submit<int>([] { return 3; }); // → worker 0 队列（其容量为 2）
    // 两个队列合计已有 3 个任务，达到全局容量，再提交应丢弃
    std::atomic<int> ran { 0 };
    auto discarded = pool.submit<void>([&ran] { ran.fetch_add(1); });
    CHECK_THROWS_AS(discarded.get(), std::future_error);

    p0.unblock();
    p1.unblock();
    blocked0.get();
    blocked1.get();
    CHECK(a.get() == 1);
    CHECK(b.get() == 2);
    CHECK(c.get() == 3);
    CHECK(ran.load() == 0);
}

TEST_CASE("工作窃取：空闲 worker 会从其他队列窃取任务", "[common][threadpool]")
{
    X::ThreadPool pool(2, 4);

    BlockingProbe probe;
    auto blocked = pool.submit<void>(probe.makeTask()); // → worker 0 并阻塞之
    probe.waitEntered();

    auto a = pool.submit<int>([] { return 1; }); // → worker 1，由其自行执行
    CHECK(a.get() == 1);

    auto b = pool.submit<int>([] { return 2; }); // → worker 0 队列（worker 0 仍被阻塞）
    // worker 1 空闲时限时等待、超时后重新窃取 b
    auto status = b.wait_for(std::chrono::milliseconds(1000));

    probe.unblock();
    blocked.get();
    REQUIRE(status == std::future_status::ready);
    CHECK(b.get() == 2);
}

TEST_CASE("多 worker 并发处理任务", "[common][threadpool]")
{
    X::ThreadPool pool(2, 20);
    std::atomic<int> sum { 0 };

    std::vector<std::future<void>> futures;
    futures.reserve(20);
    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.submit<void>([&sum, i] { sum.fetch_add(i); }));
    }
    for (auto& f : futures) {
        f.get();
    }

    CHECK(sum.load() == 190); // 0 + 1 + ... + 19
}

TEST_CASE("stop 后 worker 排空已入队任务并退出", "[common][threadpool]")
{
    BlockingProbe probe;
    X::ThreadPool pool(1, 2);

    auto blocked = pool.submit<void>(probe.makeTask()); // 占住唯一 worker
    probe.waitEntered();

    auto a = pool.submit<int>([] { return 1; }); // 队列中积压的任务
    auto b = pool.submit<int>([] { return 2; });

    pool.stop(); // 请求停止：worker 应排空 a、b 后退出

    probe.unblock();
    blocked.get();
    CHECK(a.get() == 1);
    CHECK(b.get() == 2);
}
