# os — OS 抽象

头文件：
- `#include <zero/os/os.h>` — 主机名、用户名、管道
- `#include <zero/os/process.h>` — 进程管理
- `#include <zero/os/net.h>` — 网络接口
- `#include <zero/os/stat.h>` — 系统 CPU/内存统计
- `#include <zero/os/resource.h>` — RAII OS 句柄

命名空间：`zero::os`、`zero::os::process`、`zero::os::net`、`zero::os::stat`

---

## 系统工具 (`os.h`)

```cpp
std::string host = zero::os::hostname();   // 失败时抛异常
auto user = zero::os::username();          // expected<string, error_code>

// 创建管道（读端，写端）
auto [reader, writer] = zero::os::pipe(); // 失败时抛异常
```

在 Windows 上，管道使用命名管道以支持重叠 I/O。

---

## Resource / IOResource (`resource.h`)

`Resource` 是对 OS 句柄的 RAII 封装（Unix 上为文件描述符，Windows 上为 `HANDLE`）。

`Resource` 的所有方法失败时均抛异常——只要句柄有效，这些操作在正常情况下不会失败。

```cpp
zero::os::Resource r = ...;
Resource::duplicateFrom(fd); // 静态方法：复制原生句柄，返回独立的新 Resource（原句柄不受影响）
r.get();                  // Native——底层句柄
r.valid();                // bool
r.duplicate();            // Resource（失败时抛异常）
r.isInheritable();        // bool（失败时抛异常）
r.setInheritable(true);
r.close();                // void（失败时抛异常）
auto fd = r.release();    // 释放所有权
```

`IOResource` 在 `Resource` 基础上实现了 `IReader`、`IWriter`、`ICloseable`、`ISeekable`、`IFileDescriptor`。与 `Resource::close()` 不同，`IOResource::close()` 实现了 `ICloseable` 接口，返回 `expected<void, error_code>`。`ICloseable::close()` 返回 `expected` 是因为该接口是抽象的，上层实现（如 TLS）在关闭时可能产生预期内的错误。`Resource::close()` 不实现 `ICloseable`——关闭原生 OS 句柄时发生的任何失败均属于代码错误（如 `EBADF`）或极端的系统级错误（如 `EIO`），而非预期的运行时状况，因此以异常形式抛出。信号打断已在内部处理：根据 POSIX 标准，`close` 被信号打断时文件描述符已关闭，不视为错误。

---

## 进程管理 (`process.h`)

> iOS 构建会定义 `ZERO_PROCESS_PARTIAL_API`，仅 `currentProcessID()` 可用。

### 查询进程

```cpp
using namespace zero::os::process;

// 当前进程
Process me = self(); // 失败时抛异常

// 按 PID 打开
auto proc = open(1234); // expected<Process, error_code>

// 列出所有 PID
std::list<ID> pids = all(); // 失败时抛异常
```

### 进程信息

```cpp
proc->pid();        // ID
proc->ppid();       // expected<ID, error_code>
proc->name();       // expected<string, error_code>
proc->exe();        // expected<path, error_code>
proc->cwd();        // expected<path, error_code>
proc->cmdline();    // expected<vector<string>, error_code>
proc->envs();       // expected<map<string,string>, error_code>
proc->startTime();  // expected<time_point, error_code>
proc->cpu();        // expected<CPUTime{user,system}, error_code>
proc->memory();     // expected<MemoryStat{rss,vms}, error_code>
proc->io();         // expected<IOStat{readBytes,writeBytes}, error_code>
proc->user();       // expected<string, error_code>
proc->kill();       // expected<void, error_code>
```

### 启动进程

`Command` 是一个构建器，用于配置子进程的启动方式。

```cpp
using namespace zero::os::process;

// 运行并等待
auto status = Command{"/usr/bin/ls"}
    .arg("-la")
    .currentDirectory("/tmp")
    .status();

// 捕获输出——output() 默认将 stdin 设为 null，stdout 和 stderr 设为 piped
auto out = Command{"/usr/bin/echo"}
    .arg("hello")
    .output();
// out->status、out->out（vector<byte>）、out->err

// 完整控制（通过 ChildProcess）
auto child = Command{"/path/to/prog"}
    .arg("--flag")
    .env("KEY", "VALUE")
    .stdInput(Command::Stdio::piped())
    .stdOutput(Command::Stdio::piped())
    .spawn();

if (child) {
    child->stdInput()->write(input_data);
    child->stdInput()->close();
    auto bytes = child->stdOutput()->readAll();
    ExitStatus es = child->wait();
    es.success(); // bool
    es.code();    // optional<int>
}
```

`Stdio` 工厂方法：`null()`、`inherit()`、`piped()`、`from(IOResource)`。

**仅 POSIX：** `setSID()`、`arg0(name)`、`processGroup(pgid)`、`uid(uid)`、`gid(gid)`、`groups(gids)`、`preExec(f)`。

**仅 Windows：** `creationFlags(DWORD)`、`showWindow(WORD)`、`rawAttribute(attr, value, size)`。

### PseudoConsole（伪终端）

```cpp
auto pty = PseudoConsole::make(/*rows=*/24, /*cols=*/80);
if (pty) {
    pty->resize(40, 120);
    auto child = pty->spawn(Command{"/bin/bash"});
    auto &master = pty->master(); // 用于读写 PTY 的 IOResource
}
```

---

## 网络接口 (`net.h`)

```cpp
auto ifaces = zero::os::net::interfaces(); // map<string, Interface>，失败时抛异常

for (auto &[name, iface] : ifaces) {
    // iface.mac、iface.addresses（vector<Address>）
    for (auto &addr : iface.addresses) {
        // std::visit 处理 IfAddress4 或 IfAddress6
    }
}
```

IP 类型别名：`IPv4 = std::array<std::byte, 4>`、`IPv6 = std::array<std::byte, 16>`、`IP = std::variant<IPv4, IPv6>`。

常量：`LocalhostIPv4`、`BroadcastIPv4`、`UnspecifiedIPv4`、`LocalhostIPv6`、`UnspecifiedIPv6`。

`stringify(span<byte,4>)` → 点分十进制；`stringify(span<byte,16>)` → 冒号十六进制。

---

## 系统统计 (`stat.h`)

```cpp
auto cpu = zero::os::stat::cpu();     // CPUTime{user, system, idle}，失败时抛异常
auto mem = zero::os::stat::memory();  // MemoryStat{total, used, available, free, usedPercent}，失败时抛异常
```

---

## 注意事项

- `self()`、`all()`、`hostname()`、`pipe()`、`interfaces()`、`cpu()`、`memory()` 在失败时抛异常（直接返回结果类型，而非 `expected`）。适用于失败属于真正异常情况的场景。
- `open(pid)` 返回 `expected`，因为对任意 PID 的操作是常规的可失败操作。
- `Process` 上的进程信息方法均返回 `expected`——平台 API 可能因权限不足等原因失败。
