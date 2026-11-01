# common

`common` 是一个轻量、**header-only** 的 C++20 通用工具库，供兄弟项目通过 xmake 复用（`add_deps("common")`）。所有工具定义在 `X` 命名空间下（`XType.hpp` 的别名 / 宏 / 内存字面量在**全局**命名空间），并编译为一个静态库供下游链接。

## 特性

- **C++20**（`-std=c++20`），大量使用 `constexpr`、`requires`、`concept`、`std::stop_token` 等现代特性
- **Header-only**：当前所有头文件均为模板 / `constexpr` 实现，`src/` 预留后续非模板实现，无需修改构建脚本即可接入新头文件
- 内建 **Catch2** 测试与 **ThreadPool** 性能基准

## 组件

| 头文件 | 说明 |
| --- | --- |
| [`XType.hpp`](include/XType.hpp) | 整数类型别名（`u8`…`s64`）、智能指针别名（`Sptr` / `Uptr` / `Wptr`）、`INVALID8`…`INVALID64` / `MAX_U8`…`MAX_U64` 宏，以及 `ImmutableMemorySize` + `_b` / `_kb` / `_mb` / `_gb` 内存大小字面量 |
| [`Noncopyable.hpp`](include/Noncopyable.hpp) | `X::NonCopyable`（禁拷贝、可移动）、`X::NonCopyOrMoveable`（禁拷贝禁移动） |
| [`Singleton.hpp`](include/Singleton.hpp) | CRTP 单例 `X::Singleton<T>` 与构造令牌单例 `X::SingletonWithConstructToken<T>` |
| [`StringUtil.hpp`](include/StringUtil.hpp) | `X::StringUtil::hashString`，FNV-1a 哈希（0 偏移基、64 位 FNV 素数 `0x100000001B3ULL`，支持自定义素数） |
| [`InlineBuffer.hpp`](include/InlineBuffer.hpp) | `X::InlineBuffer<N>`，小对象优化（SBO）缓冲区：栈上分配，超长时自动切换堆分配 |
| [`BoundedQueue.hpp`](include/BoundedQueue.hpp) | `X::BoundedQueue<T>`，线程安全有界队列，支持阻塞 / 限时 / 非阻塞入队出队，由 `std::stop_token` 控制关闭 |
| [`ThreadPool.hpp`](include/ThreadPool.hpp) | `X::ThreadPool`，工作窃取线程池：每个 worker 持有本地有界队列，统一全局容量控制 |

## 使用方式

在项目的 `xmake.lua` 中依赖 `common`，即可直接包含头文件：

```lua
add_deps("common")
```

```cpp
#include "Singleton.hpp"
#include "StringUtil.hpp"
#include "ThreadPool.hpp"

class MyManager : public X::Singleton<MyManager> {
protected:
    MyManager() = default;
    friend class X::Singleton<MyManager>;
};

// 单例
auto manager = MyManager::getInstance(); // std::shared_ptr<MyManager>
auto& ref     = MyManager::getInstanceRef();

// FNV-1a 哈希
constexpr auto h = X::StringUtil::hashString("hello");

// 工作窃取线程池：2 个线程，全局总容量 64
X::ThreadPool pool(2, 64);
auto future = pool.submit<int>([] { return 42; });
int answer  = future.get(); // 42
```

## 构建与测试

```sh
xmake                  # 构建 common 静态库
xmake test             # 构建并运行 Catch2 测试（common_test）
xmake run common_test  # 直接运行测试（或 ./build/linux/x86_64/release/common_test）
```

构建时 `plugin.compile_commands.autoupdate` 会自动生成 `.vscode/compile_commands.json`，供 clangd / IDE 使用。

### 性能基准

```sh
xmake build threadpool_bench
./build/linux/x86_64/release/threadpool_bench [任务数]
```

## 项目结构

```
include/         公共头文件（header-only；模板 / constexpr）
  Noncopyable.hpp   X::NonCopyable, X::NonCopyOrMoveable
  Singleton.hpp     X::Singleton<T>, X::SingletonWithConstructToken<T>
  StringUtil.hpp    X::StringUtil::hashString（FNV-1a）
  XType.hpp         整数 / 智能指针别名、宏、内存大小字面量
  InlineBuffer.hpp  X::InlineBuffer<N>（SBO 缓冲区）
  BoundedQueue.hpp  X::BoundedQueue<T>（线程安全有界队列）
  ThreadPool.hpp    X::ThreadPool（工作窃取线程池）
src/             预留非模板实现（目前为空）
test/            Catch2 测试套件，每个头文件对应一个 .cpp
bench/           ThreadPool 性能基准
xmake.lua        根构建脚本（定义 common 静态目标）
```

## 约定

- 命名空间：库代码位于 `X`，`StringUtil` 函数位于 `X::StringUtil`；`XType.hpp` 的别名 / 宏 / 内存字面量位于**全局**命名空间
- 注释与测试名使用**中文**
- 测试使用 Catch2，标签形如 `[common][<area>]`；单例测试每个用例使用独立派生类型（实例为进程全局）
- 编译期属性用 `static_assert` 校验，运行期行为用 `CHECK`
- 测试目标开启 `-Wall -Wextra`，保持零警告
