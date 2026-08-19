# zero

A cross-platform C++23 utility library providing foundational building blocks for systems programming: async/promise primitives, lock-free data structures, thread-safe channels, OS abstractions, I/O interfaces, logging, error handling, encoding, and more.

**Version:** 1.3.0 | **Language:** C++23 | **License:** See [LICENSE](LICENSE)

[中文文档](README.zh.md)

---

## Features

| Module | Description |
|---|---|
| [async](doc/async.md) | Promise/Future/SemiFuture chains with `all`, `any`, `race`, `allSettled` |
| [concurrent](doc/concurrent.md) | Thread-safe bounded channel (MPMC) with blocking and non-blocking send/receive |
| [atomic](doc/atomic.md) | Lock-free circular buffer, futex-based event primitive |
| [io](doc/io.md) | I/O interfaces, buffered reader/writer, binary LE/BE read/write |
| [os](doc/os.md) | Process management, pipe, hostname, network interfaces, system stats |
| [error](doc/error.md) | `std::error_code`/`std::expected` macros, `guard()`, `capture()` |
| [encoding](doc/encoding.md) | Base64 and hex encode/decode |
| [log](doc/log.md) | Async multi-sink logger with level/tag filtering |
| [strings](doc/strings.md) | String utilities, command-line parser, environment variables |
| [filesystem](doc/filesystem.md) | `std::expected`-wrapped filesystem API |
| [cache](doc/cache.md) | LRU cache |
| [meta](doc/meta.md) | `FunctionTraits`, concepts (`Implements`, `OwnerOf`, etc.) |
| [utility](doc/utility.md) | `Z_DEFER`, `Z_EXPECT`/`Z_TRY` propagation macros, monadic helpers |

---

## Requirements

- **Compiler:** GCC 15+, LLVM 18+ (Clang/Apple Clang), or MSVC 19.38+
- **CMake:** 3.25 or newer
- **vcpkg:** for dependency management (`fmt`, optionally `libiconv` on Windows)
- **C++ standard:** 23

---

## Installation

### 1. Add as vcpkg dependency

Create a `vcpkg-configuration.json` to point vcpkg at the custom registry:

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

Then in your `vcpkg.json`:

```json
{
  "dependencies": ["zero"]
}
```

Or reference the source directory directly via `add_subdirectory`.

### 2. Link in CMake

```cmake
find_package(zero CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE zero::zero)
```

---

## Build

Clone the repository, then use one of the provided CMake presets:

```bash
# Configure and build (debug)
cmake --preset debug
cmake --build --preset debug

# Run tests
ctest --preset debug

# Release build
cmake --preset release
cmake --build --preset release
```

Available presets: `debug`, `debug-asan` (AddressSanitizer), `relwithdebinfo`, `release`.

---

## Quick Start

### Error propagation with `Z_EXPECT`

```cpp
#include <zero/expect.h>
#include <zero/filesystem.h>

std::expected<std::string, std::error_code> readConfig(const std::filesystem::path &path) {
    auto data = zero::filesystem::readString(path);
    Z_EXPECT(data);
    return *data;
}
```

### Async promise chains

```cpp
#include <zero/async/promise.h>

using namespace zero::async::promise;

auto future = Future<int>::resolved(42)
    .then([](int v) { return v * 2; })
    .then([](int v) -> Future<std::string> {
        // async continuation
        Promise<std::string> p;
        auto f = p.getFuture().via(InlineExecutor::instance());
        p.resolve(std::to_string(v));
        return f;
    });

std::move(future).get(); // blocks until ready
```

### Channel communication

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
    // use *val
}
producer.join();
```

### Spawning a child process

```cpp
#include <zero/os/process.h>

// output() defaults stdin to null, stdout and stderr to piped
auto result = zero::os::process::Command{"/usr/bin/echo"}
    .arg("hello")
    .output();

if (result)
    // result->out contains stdout bytes
```

---

## Platform Support

| Platform | Architecture | Status |
|---|---|---|
| Linux (glibc, Clang 19 + libc++) | x86-64 | Full support |
| Linux static (musl, GCC 15.2) | x86-64 | Full support |
| Windows | x86, x86-64 | Full support |
| macOS | arm64, x86-64 | Full support |
| iOS | arm64 | Partial (no full process API) |
| Android (NDK) | arm64 | Partial (no tests) |
| OpenHarmony (OHOS) | arm64 | Partial (no tests) |
