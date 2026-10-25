-- common 通用工具库构建脚本
--
-- 提供一组跨项目复用的基础工具（Noncopyable、XType、Singleton、StringUtil 等），
-- 编译为静态库，供下游目标链接使用：
--   add_deps("common") 后即可 #include "Noncopyable.hpp" / "XType.hpp" 等。
--
-- 说明：当前头文件均为模板 / constexpr 实现，src/ 预留后续非模板实现；
-- 源码加入 src/ 后会被自动纳入静态库编译。

-- 编译数据库（供 IDE / clangd 使用）
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

target("common")
    set_kind("static")

    -- 对外暴露的头文件与包含目录
    add_headerfiles("include/*.hpp")
    add_includedirs("include", {public = true})

    -- 仅在存在源码时编译，避免空 src/ 触发 "cannot match add_files" 警告
    local sources = os.files("src/*.cpp")
    if #sources > 0 then
        add_files(sources)
    end

    -- Singleton.hpp 使用 requires / std::constructible_from 等 C++20 特性
    set_languages("c++20")
target_end()

-- 测试目标（Catch2，定义在 test/xmake.lua 中）
includes("test")
