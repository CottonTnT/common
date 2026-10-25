// XType 基础类型别名、宏常量与内存大小字面量测试
//
// 注意：XType.hpp 中的别名 / 宏 / ImmutableMemorySize 均定义在全局命名空间。

#include <catch2/catch_test_macros.hpp>

#include "XType.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

TEST_CASE("XType 整型别名", "[common][xtype]")
{
    static_assert(std::is_same_v<u8, std::uint8_t>);
    static_assert(std::is_same_v<s8, std::int8_t>);
    static_assert(std::is_same_v<u16, std::uint16_t>);
    static_assert(std::is_same_v<s16, std::int16_t>);
    static_assert(std::is_same_v<u32, std::uint32_t>);
    static_assert(std::is_same_v<s32, std::int32_t>);
    static_assert(std::is_same_v<u64, std::uint64_t>);
    static_assert(std::is_same_v<s64, std::int64_t>);

    static_assert(sizeof(u8) == 1);
    static_assert(sizeof(u16) == 2);
    static_assert(sizeof(u32) == 4);
    static_assert(sizeof(u64) == 8);
    CHECK(true);
}

TEST_CASE("XType 智能指针别名", "[common][xtype]")
{
    static_assert(std::is_same_v<Sptr<int>, std::shared_ptr<int>>);
    static_assert(std::is_same_v<Uptr<int>, std::unique_ptr<int>>);
    static_assert(std::is_same_v<Wptr<int>, std::weak_ptr<int>>);

    Uptr<int> u = std::make_unique<int>(42);
    CHECK(*u == 42);
}

TEST_CASE("XType 无效值与上限宏", "[common][xtype]")
{
    CHECK(INVALID8 == 0xFF);
    CHECK(INVALID16 == 0xFFFF);
    CHECK(INVALID32 == 0xFFFFFFFFu);
    CHECK(INVALID64 == ~0ULL);

    CHECK(MAX_U8 == 0xFF);
    CHECK(MAX_U16 == 0xFFFF);
    CHECK(MAX_U32 == 0xFFFFFFFFu);
    CHECK(MAX_U64 == ~0ULL);
}

TEST_CASE("XType 内存大小字面量", "[common][xtype]")
{
    static_assert(std::is_same_v<decltype(1_b), ImmutableMemorySize>);

    CHECK(static_cast<std::size_t>(1_b) == 1u);
    CHECK(static_cast<std::size_t>(3_b) == 3u);
    CHECK(static_cast<std::size_t>(1_kb) == 1024u);
    CHECK(static_cast<std::size_t>(2_kb) == 2048u);
    CHECK(static_cast<std::size_t>(1_mb) == 1024u * 1024u);
    CHECK(static_cast<std::size_t>(1_gb) == 1024u * 1024u * 1024u);
}
