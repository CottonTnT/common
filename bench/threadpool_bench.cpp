// ThreadPool 性能基准测试
//
// 测量不同线程数下提交并执行任务的吞吐（任务/秒），并与单线程串行基线对比。
// 容量设为任务数，保证全部入队不丢弃；请在 release 模式下运行，结果才有意义。
//
// 构建运行：
//   xmake build threadpool_bench
//   ./build/linux/x86_64/release/threadpool_bench [任务数]

#include "ThreadPool.hpp"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// 提交 n 个任务并等待全部完成，返回耗时（秒）
auto runPool(std::size_t threads, std::size_t n, const std::function<void()>& task)
    -> double
{
    X::ThreadPool pool(threads, n); // 容量 = 任务数，保证全部入队不丢弃

    std::vector<std::future<void>> futures;
    futures.reserve(n);

    auto start = Clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        futures.push_back(pool.submit<void>(task));
    }
    for (auto& f : futures) {
        f.get();
    }
    auto end = Clock::now();

    return std::chrono::duration<double>(end - start).count();
}

// 单线程串行基线：直接调用任务 n 次
auto runSerial(std::size_t n, const std::function<void()>& task)
    -> double
{
    auto start = Clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        task();
    }
    auto end = Clock::now();
    return std::chrono::duration<double>(end - start).count();
}

void printRow(const std::string& label, double seconds, std::size_t n)
{
    std::printf("  %-12s %12.2f %16.0f\n", label.c_str(), seconds * 1000.0,
        static_cast<double>(n) / seconds);
}

} // namespace

int main(int argc, char** argv)
{
    const std::size_t n = (argc > 1)
        ? static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10))
        : 200'000;

    std::size_t hw = std::thread::hardware_concurrency();
    if (hw == 0) {
        hw = 4;
    }

    // 待测线程数（去重，含硬件并发数）
    const std::size_t candidates[] = { 1, 2, 4, 8, hw };
    std::vector<std::size_t> threads;
    for (std::size_t t : candidates) {
        bool exists = false;
        for (auto x : threads) {
            if (x == t) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            threads.push_back(t);
        }
    }

    std::printf("ThreadPool 性能基准（任务数 = %zu，硬件并发 = %zu）\n", n, hw);

    // 1) 空任务：真正 no-op，测量提交 + 调度的纯开销（无共享状态）
    {
        std::printf("\n== 空任务（no-op）==\n");
        std::printf("  %-12s %12s %16s\n", "线程数", "耗时(ms)", "吞吐(任务/秒)");

        std::function<void()> task = [] { };

        printRow("串行", runSerial(n, task), n);
        for (auto t : threads) {
            printRow(std::to_string(t) + " 线程", runPool(t, n, task), n);
        }
    }

    // 2) 轻任务：本地累加（volatile 防止优化），无跨线程竞争，测量小任务吞吐
    {
        std::printf("\n== 轻任务（本地累加 1..1000）==\n");
        std::printf("  %-12s %12s %16s\n", "线程数", "耗时(ms)", "吞吐(任务/秒)");

        std::function<void()> task = [] {
            volatile std::size_t s = 0;
            for (std::size_t i = 1; i <= 1000; ++i) {
                s += i;
            }
            (void)s;
        };

        printRow("串行", runSerial(n, task), n);
        for (auto t : threads) {
            printRow(std::to_string(t) + " 线程", runPool(t, n, task), n);
        }
    }

    // 3) 睡眠任务：每任务睡 1 秒，测量长任务下的扩展性（调度开销可忽略）
    {
        std::printf("\n== 睡眠任务（每任务 1 秒）==\n");
        std::printf("  %-12s %12s %16s\n", "线程数", "耗时(ms)", "吞吐(任务/秒)");

        std::function<void()> task = [] { std::this_thread::sleep_for(std::chrono::seconds(1)); };

        // 串行基线：2 个任务串行 ≈ 2 秒
        printRow("串行(2任务)", runSerial(2, task), 2);

        // 每个 worker 分到 2 个任务，总耗时理论上恒为 ~2 秒
        for (auto t : threads) {
            const std::size_t cnt = t * 2;
            printRow(std::to_string(t) + " 线程", runPool(t, cnt, task), cnt);
        }
    }

    std::printf("\n");
    return 0;
}
