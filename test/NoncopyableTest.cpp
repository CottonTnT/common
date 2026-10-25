// NonCopyable / NonCopyOrMoveable 语义测试
//
// 两个基类通过删除拷贝（以及可选的移动）构造/赋值来约束派生类，
// 这里通过派生类型在编译期断言拷贝/移动语义，并在运行期验证移动可用。

#include <catch2/catch_test_macros.hpp>

#include "Noncopyable.hpp"

#include <type_traits>
#include <utility>

namespace {

struct TestNonCopyable : X::NonCopyable {
    TestNonCopyable() = default;
};

struct TestNonCopyOrMoveable : X::NonCopyOrMoveable {
    TestNonCopyOrMoveable() = default;
};

} // namespace

TEST_CASE("NonCopyable 禁止拷贝但允许移动", "[common][noncopyable]")
{
    static_assert(!std::is_copy_constructible_v<TestNonCopyable>);
    static_assert(!std::is_copy_assignable_v<TestNonCopyable>);
    static_assert(std::is_move_constructible_v<TestNonCopyable>);
    static_assert(std::is_move_assignable_v<TestNonCopyable>);

    // 运行期验证移动构造 / 移动赋值可用
    TestNonCopyable a;
    TestNonCopyable b = std::move(a);
    b = std::move(a);
    CHECK(true);
}

TEST_CASE("NonCopyOrMoveable 禁止拷贝与移动", "[common][noncopyable]")
{
    static_assert(!std::is_copy_constructible_v<TestNonCopyOrMoveable>);
    static_assert(!std::is_copy_assignable_v<TestNonCopyOrMoveable>);
    static_assert(!std::is_move_constructible_v<TestNonCopyOrMoveable>);
    static_assert(!std::is_move_assignable_v<TestNonCopyOrMoveable>);

    TestNonCopyOrMoveable a;
    (void)a;
    CHECK(true);
}
