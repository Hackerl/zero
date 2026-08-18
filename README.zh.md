# zero

一个跨平台的 C++23 工具库，为系统编程提供基础构建模块：异步/Promise 原语、无锁数据结构、线程安全的 Channel、OS 抽象、I/O 接口、日志、错误处理、编解码等。

**版本：** 1.2.0 | **语言标准：** C++23 | **许可证：** 见 [LICENSE](LICENSE)

[English Documentation](README.md)

---

## 功能模块

| 模块 | 描述 |
|---|---|
| [async](doc/zh/async.md) | Promise/Future/SemiFuture 链式调用，支持 `all`、`any`、`race`、`allSettled` |
| [concurrent](doc/zh/concurrent.md) | 线程安全的有界 Channel（MPMC），支持阻塞与非阻塞收发 |
| [atomic](doc/zh/atomic.md) | 无锁环形缓冲区、基于 futex 的事件原语 |
| [io](doc/zh/io.md) | I/O 接口、带缓冲的读写器、二进制大端/小端读写 |
| [os](doc/zh/os.md) | 进程管理、管道、主机名、网络接口、系统统计 |
| [error](doc/zh/error.md) | `std::error_code`/`std::expected` 宏、`guard()`、`capture()` |
| [encoding](doc/zh/encoding.md) | Base64 与十六进制编解码 |
| [log](doc/zh/log.md) | 异步多 Sink 日志，支持级别/标签过滤 |
| [strings](doc/zh/strings.md) | 字符串工具、命令行解析器、环境变量 |
| [filesystem](doc/zh/filesystem.md) | 基于 `std::expected` 封装的文件系统 API |
| [cache](doc/zh/cache.md) | LRU 缓存 |
| [meta](doc/zh/meta.md) | `FunctionTraits`、概念约束（`Implements`、`OwnerOf` 等） |
| [utility](doc/zh/utility.md) | `Z_DEFER`、`Z_EXPECT`/`Z_TRY` 传播宏、单子辅助工具 |

---

## 环境要求

- **编译器：** GCC 15+、LLVM 18+（Clang/Apple Clang）或 MSVC 19.38+
- **CMake：** 3.25 或更高版本
- **vcpkg：** 用于依赖管理（`fmt`，Windows 下可选 `libiconv`）
- **C++ 标准：** 23

---

## 安装方式

### 1. 通过 vcpkg 添加依赖

创建 `vcpkg-configuration.json`，将 vcpkg 指向自定义注册表：

```json
{
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/Hackerl/vcpkg-registry",
      "baseline": "eaffda9e8706fa2d9db5546cff59b124691cd0af",
      "packages": ["zero"]
    }
  ]
}
```

然后在 `vcpkg.json` 中：

```json
{
  "dependencies": ["zero"]
}
```

或者通过 `add_subdirectory` 直接引用源码目录。

### 2. 在 CMake 中链接

```cmake
find_package(zero CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE zero::zero)
```

---

## 构建

克隆仓库后，使用提供的 CMake 预设之一：

```bash
# 配置并构建（调试模式）
cmake --preset debug
cmake --build --preset debug

# 运行测试
ctest --preset debug

# Release 构建
cmake --preset release
cmake --build --preset release
```

可用预设：`debug`、`debug-asan`（AddressSanitizer）、`relwithdebinfo`、`release`。

---

## 快速开始

### 使用 `Z_EXPECT` 传播错误

```cpp
#include <zero/expect.h>
#include <zero/filesystem.h>

std::expected<std::string, std::error_code> readConfig(const std::filesystem::path &path) {
    auto data = zero::filesystem::readString(path);
    Z_EXPECT(data);
    return *data;
}
```

### 异步 Promise 链

```cpp
#include <zero/async/promise.h>

using namespace zero::async::promise;

auto future = Future<int>::resolved(42)
    .then([](int v) { return v * 2; })
    .then([](int v) -> Future<std::string> {
        Promise<std::string> p;
        auto f = p.getFuture().via(InlineExecutor::instance());
        p.resolve(std::to_string(v));
        return f;
    });

std::move(future).get(); // 阻塞直到完成
```

### Channel 通信

```cpp
#include <zero/concurrent/channel.h>

auto [sender, receiver] = zero::concurrent::channel<int>(8);

std::thread producer([s = std::move(sender)]() mutable {
    for (int i = 0; i < 10; ++i)
        s.send(i);
});

while (true) {
    auto val = receiver.receive();
    if (!val) break; // Disconnected
    // 使用 *val
}
producer.join();
```

### 启动子进程

```cpp
#include <zero/os/process.h>

// output() 默认将 stdin 设为 null，stdout 和 stderr 设为 piped
auto result = zero::os::process::Command{"/usr/bin/echo"}
    .arg("hello")
    .output();

if (result)
    // result->out 包含标准输出的字节数据
```

---

## 平台支持

| 平台 | 架构 | 状态 |
|---|---|---|
| Linux（glibc，Clang 19 + libc++）| x86-64 | 完全支持 |
| Linux 静态链接（musl，GCC 15.2）| x86-64 | 完全支持 |
| Windows | x86、x86-64 | 完全支持 |
| macOS | arm64、x86-64 | 完全支持 |
| iOS | arm64 | 部分支持（无完整进程 API）|
| Android（NDK）| arm64 | 部分支持（无测试）|
| OpenHarmony（OHOS）| arm64 | 部分支持（无测试）|
