// BoundedQueue 有界阻塞队列测试
//
// 覆盖：阻塞 push/pop（构造时传入 std::stop_token 进行关闭控制）、
// 非阻塞 tryPush/tryPop 与查询接口。

#include <catch2/catch_test_macros.hpp>

#include "BoundedQueue.hpp"

#include <chrono>
#include <stop_token>
#include <thread>

TEST_CASE("tryPush 在未满时成功并计数", "[common][boundedqueue]")
{
    X::BoundedQueue<int> q(2);
    CHECK(q.tryPush(1) == X::BoundedQueueErrc::SUCCESS);
    CHECK(q.tryPush(2) == X::BoundedQueueErrc::SUCCESS);
    CHECK(q.size() == 2);
    CHECK(q.full());
}

TEST_CASE("tryPush 满时返回 FULL 且不改变内容", "[common][boundedqueue]")
{
    X::BoundedQueue<int> q(1);
    CHECK(q.tryPush(1) == X::BoundedQueueErrc::SUCCESS);
    CHECK(q.tryPush(2) == X::BoundedQueueErrc::FULL);
    CHECK(q.size() == 1);
    CHECK(q.full());
}

TEST_CASE("tryPop 空时返回 EMPTY", "[common][boundedqueue]")
{
    X::BoundedQueue<int> q(1);
    int v = 0;
    CHECK(q.empty());
    CHECK(q.tryPop(v) == X::BoundedQueueErrc::EMPTY);
}

TEST_CASE("tryPop 按先进先出顺序出队", "[common][boundedqueue]")
{
    X::BoundedQueue<int> q(2);
    CHECK(q.tryPush(10) == X::BoundedQueueErrc::SUCCESS);
    CHECK(q.tryPush(20) == X::BoundedQueueErrc::SUCCESS);

    int v = 0;
    CHECK(q.tryPop(v) == X::BoundedQueueErrc::SUCCESS);
    CHECK(v == 10);
    CHECK(q.size() == 1);

    CHECK(q.tryPop(v) == X::BoundedQueueErrc::SUCCESS);
    CHECK(v == 20);
    CHECK(q.empty());
}

TEST_CASE("capacity 返回构造容量", "[common][boundedqueue]")
{
    X::BoundedQueue<int> q(3);
    CHECK(q.capacity() == 3);
}

TEST_CASE("阻塞 push 在队列满时等待直到有空间", "[common][boundedqueue]")
{
    std::stop_source ss;
    X::BoundedQueue<int> q(1, ss.get_token());

    CHECK(q.push(1) == X::BoundedQueueErrc::SUCCESS);

    // 后台线程稍后取出一个元素，为阻塞的 push 腾出空间
    std::thread consumer([&q] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int v = 0;
        q.pop(v);
    });

    CHECK(q.push(2) == X::BoundedQueueErrc::SUCCESS); // 阻塞直到消费者取出元素
    consumer.join();
    CHECK(q.size() == 1);
}

TEST_CASE("stop 请求后阻塞 pop 在空队列返回 STOPPED", "[common][boundedqueue]")
{
    std::stop_source ss;
    X::BoundedQueue<int> q(1, ss.get_token());

    X::BoundedQueueErrc result = X::BoundedQueueErrc::SUCCESS;
    std::thread popper([&q, &result] {
        int v = 0;
        result = q.pop(v);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 确保 popper 已阻塞
    ss.request_stop(); // 唤醒 popper
    popper.join();
    CHECK(result == X::BoundedQueueErrc::STOPPED);
}

TEST_CASE("stop 请求后阻塞 push 在满队列返回 STOPPED", "[common][boundedqueue]")
{
    std::stop_source ss;
    X::BoundedQueue<int> q(1, ss.get_token());

    CHECK(q.push(1) == X::BoundedQueueErrc::SUCCESS); // 填满

    X::BoundedQueueErrc result = X::BoundedQueueErrc::SUCCESS;
    std::thread pusher([&q, &result] {
        result = q.push(2); // 阻塞直到空间或 stop
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ss.request_stop();
    pusher.join();
    CHECK(result == X::BoundedQueueErrc::STOPPED);
}

TEST_CASE("stop 请求后仍可排空已入队元素", "[common][boundedqueue]")
{
    std::stop_source ss;
    X::BoundedQueue<int> q(2, ss.get_token());

    CHECK(q.push(1) == X::BoundedQueueErrc::SUCCESS);
    CHECK(q.push(2) == X::BoundedQueueErrc::SUCCESS);
    ss.request_stop();

    int v = 0;
    CHECK(q.pop(v) == X::BoundedQueueErrc::SUCCESS); // 已入队的仍可取
    CHECK(v == 1);
    CHECK(q.pop(v) == X::BoundedQueueErrc::SUCCESS);
    CHECK(v == 2);
    CHECK(q.pop(v) == X::BoundedQueueErrc::STOPPED); // 空且 stop，返回 STOPPED
}
