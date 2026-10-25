// StringUtil::hashString（FNV-1a，偏移 0）测试
//
// 实现：hash_value 从 0 开始，对每个字节执行 (hash ^ byte) * prime，
// prime 为 64 位 FNV 质数 0x100000001B3。

#include <catch2/catch_test_macros.hpp>

#include "StringUtil.hpp"

TEST_CASE("StringUtil::hashString 空串为 0", "[common][stringutil]")
{
    CHECK(X::StringUtil::hashString("") == 0ULL);
    CHECK(X::StringUtil::hashString("", 0x100000001B3ULL) == 0ULL);
}

TEST_CASE("StringUtil::hashString FNV-1a 基准向量", "[common][stringutil]")
{
    CHECK(X::StringUtil::hashString("a") == 106652627936467ULL);
    CHECK(X::StringUtil::hashString("ab") == 92750402891012291ULL);
    CHECK(X::StringUtil::hashString("abc") == 5171236289873516512ULL);
    CHECK(X::StringUtil::hashString("hello") == 12121257498555280600ULL);
    CHECK(X::StringUtil::hashString("Hello, World") == 13212153343631066096ULL);
}

TEST_CASE("StringUtil::hashString 确定性", "[common][stringutil]")
{
    CHECK(X::StringUtil::hashString("common") == X::StringUtil::hashString("common"));
    CHECK(X::StringUtil::hashString("common") == 9262120998700779225ULL);
}
