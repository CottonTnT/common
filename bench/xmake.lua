-- ThreadPool 性能基准（普通可执行，无测试框架）
--
-- 构建并运行：
--   xmake build threadpool_bench
--   ./build/linux/x86_64/release/threadpool_bench [任务数]

target("threadpool_bench")
    set_kind("binary")
    add_deps("common")
    add_files("*.cpp")
    set_languages("c++20")
    add_cxflags("-Wall", "-Wextra")
    set_optimize("fastest") -- 基准测试用最高优化
target_end()
