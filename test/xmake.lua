-- common 测试目标（Catch2）
--
-- 通过 `xmake test` 运行，或直接构建 common_test 目标。
-- Catch2 由 xmake 包管理自动下载/构建并链接。

add_requires("catch2")

target("common_test")
    set_kind("binary")
    add_deps("common")
    add_files("*.cpp")
    set_languages("c++20")
    add_cxflags("-Wall", "-Wextra")
    add_packages("catch2", {components = {"main", "lib"}})
    add_tests("common_test")
target_end()
