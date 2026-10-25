# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

`common` is a small, header-only C++20 utility library meant to be reused across sibling projects (`add_deps("common")`). It exposes a handful of foundational tools under the `X` namespace, and compiles to a static library for downstream linking.

- **Language:** C++20 (`-std=c++20`)
- **Build system:** [xmake](https://xmake.io) (not CMake)
- **Tests:** Catch2, pulled in automatically by xmake's package manager

## Project layout

```
include/          Public headers (header-only; templates / constexpr)
  Noncopyable.hpp   X::NonCopyable, X::NonCopyOrMoveable
  Singleton.hpp     X::Singleton<T>, X::SingletonWithConstructToken<T>
  StringUtil.hpp    X::StringUtil::hashString (FNV-1a)
  XType.hpp         integer/smart-pointer aliases, macros, memory-size literals
src/              Reserved for future non-template implementations (currently empty)
test/             Catch2 test suites, one .cpp per header
xmake.lua         Root build script (defines the `common` static target)
test/xmake.lua    Defines the `common_test` binary target
```

`include/` is the only source of truth for the library today. The root `xmake.lua` globs `include/*.hpp` via `add_headerfiles` and only adds `src/*.cpp` files if any exist, so new headers are picked up automatically and a `.cpp` dropped into `src/` is compiled into the static library with no build-file changes.

## Building and testing

```sh
xmake            # build the `common` static library
xmake test       # build + run the Catch2 `common_test` target
xmake run common_test   # run tests directly (or `./build/linux/x86_64/release/common_test`)
```

The `plugin.compile_commands.autoupdate` rule regenerates `.vscode/compile_commands.json` on every build for clangd/IDE support.

## Components and conventions

- **Namespace:** all library code lives in `X`; `StringUtil` functions live in `X::StringUtil`. Exception: `XType.hpp`'s aliases (`u8`…`s64`, `Sptr`/`Uptr`/`Wptr`), macros (`INVALID8`…`INVALID64`, `MAX_U8`…`MAX_U64`), and `ImmutableMemorySize` + `_b`/`_kb`/`_mb`/`_gb` literals are defined in the **global** namespace.
- **Comments and test names are in Chinese.** Keep new comments/tests consistent with this style.
- **`Singleton` is CRTP-based** — a type declares itself a singleton via `class Foo : public X::Singleton<Foo>`. Construction is controlled by making the constructor `protected`, and instances are held in a function-local `static` `std::shared_ptr<T>` (thread-safe, one per process). `getInstance()` returns `std::shared_ptr<T>`; `getInstanceRef()` returns `T&`.
- **`SingletonWithConstructToken`** uses a construct-token pattern: the derived type's constructor takes a private `ConstructToken` so only the base's `getInstance()` can construct it.
- **`getInstance()` is C++20-constrained** with `requires std::default_initializable<T>` (or `std::constructible_from<T, ContructToken>`), so calling it on a non-constructible type is a compile error.
- **`hashString`** implements FNV-1a with a 0 offset base and the 64-bit FNV prime `0x100000001B3ULL`; a two-argument overload accepts a custom prime.

## Tests

Each header has a matching `test/*Test.cpp` using Catch2 with tags of the form `[common][<area>]` (e.g. `[common][singleton]`). Test types that are singletons use a distinct derived type per test case because the instance is process-global and shared across cases. Compile-time properties are checked with `static_assert` (e.g. copy/move-constructibility of `NonCopyable`), runtime behavior with `CHECK`.

The test target compiles with `-Wall -Wextra`; keep it warning-clean.
