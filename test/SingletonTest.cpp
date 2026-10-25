// Singleton / SingletonWithConstructToken 单例测试
//
// 注意：Singleton 的实例是函数内 static，进程内全局唯一。
// 为避免测试用例之间相互影响，每个用例使用独立的派生类型。

#include <catch2/catch_test_macros.hpp>

#include "Singleton.hpp"

namespace {

class Counter : public X::Singleton<Counter> {
public:
    Counter() = default;

    int value() const { return value_; }
    void set(int v) { value_ = v; }

private:
    int value_ = 0;
};

class ArgInit : public X::Singleton<ArgInit> {
public:
    explicit ArgInit()
        = default;

    void init(int v) { value_ = v; }

    int value_ = 0;
};

class TokenService : public X::SingletonWithConstructToken<TokenService> {
public:
    TokenService(ConstructToken)
    {
    }

    void init(int id) { id_ = id; }

    int id() const { return id_; }

private:
    int id_;
};

} // namespace

TEST_CASE("Singleton::getInstance 返回同一实例", "[common][singleton]")
{
    auto a = Counter::getInstance();
    auto b = Counter::getInstance();
    CHECK(a.get() == b.get());
}

TEST_CASE("Singleton::getInstanceRef 返回实例引用", "[common][singleton]")
{
    Counter::getInstanceRef().set(42);
    CHECK(Counter::getInstance()->value() == 42);
    CHECK(&Counter::getInstanceRef() == Counter::getInstance().get());
}

TEST_CASE("Singleton::getInstance 转发构造参数", "[common][singleton]")
{
    auto instance = ArgInit::getInstance();
    instance->init(7);

    CHECK(ArgInit::getInstance()->value_ == 7);
}

TEST_CASE("SingletonWithConstructToken::getInstance 返回同一实例", "[common][singleton]")
{
    auto a = TokenService::getInstance();
    a->init(9);

    auto b = TokenService::getInstance();
    b->init(9);
    CHECK(a.get() == b.get());
    CHECK(a->id() == 9);
}
