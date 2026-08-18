# os — OS Abstractions

Headers:
- `#include <zero/os/os.h>` — hostname, username, pipe
- `#include <zero/os/process.h>` — process management
- `#include <zero/os/net.h>` — network interfaces
- `#include <zero/os/stat.h>` — system CPU/memory stats
- `#include <zero/os/resource.h>` — RAII OS handles

Namespaces: `zero::os`, `zero::os::process`, `zero::os::net`, `zero::os::stat`

---

## System Utilities (`os.h`)

```cpp
std::string host = zero::os::hostname();   // throws on failure
auto user = zero::os::username();          // expected<string, error_code>

// Create a pipe (read end, write end)
auto [reader, writer] = zero::os::pipe(); // throws on failure
```

On Windows, the pipe uses named pipes with overlapped I/O support.

---

## Resource / IOResource (`resource.h`)

`Resource` is a RAII wrapper around an OS handle (file descriptor on Unix, `HANDLE` on Windows).

All `Resource` methods throw on failure — these operations have no expected failure modes when the handle is valid.

```cpp
zero::os::Resource r = ...;
Resource::duplicateFrom(fd); // static: duplicate a native handle into a new owned Resource
r.get();             // Native — underlying handle
r.valid();           // bool
r.duplicate();       // Resource (throws on failure)
r.isInheritable();   // bool (throws on failure)
r.setInheritable(true);
r.close();           // void (throws on failure)
auto fd = r.release(); // release ownership
```

`IOResource` extends `Resource` with `IReader`, `IWriter`, `ICloseable`, `ISeekable`, `IFileDescriptor`. Unlike `Resource::close()`, `IOResource::close()` implements `ICloseable` and returns `expected<void, error_code>`. `ICloseable::close()` returns `expected` because higher-level closeable types (e.g. TLS) may produce expected errors during shutdown. `Resource::close()` does not implement `ICloseable` — any failure closing a raw OS handle indicates a programming error (e.g. `EBADF`) or an extreme system-level condition (e.g. `EIO`), never an expected runtime condition, so errors are thrown as exceptions. Signal interruption is handled internally; per POSIX, if `close` is interrupted by a signal the file descriptor is already closed and this is not treated as an error.

---

## Process Management (`process.h`)

> iOS builds define `ZERO_PROCESS_PARTIAL_API` — only `currentProcessID()` is available.

### Querying Processes

```cpp
using namespace zero::os::process;

// Current process
Process me = self(); // throws

// Open by PID
auto proc = open(1234); // expected<Process, error_code>

// List all PIDs
std::list<ID> pids = all(); // throws
```

### Process Information

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

### Spawning Processes

`Command` is a builder that configures how a child process is launched.

```cpp
using namespace zero::os::process;

// Simple run-and-wait
auto status = Command{"/usr/bin/ls"}
    .arg("-la")
    .currentDirectory("/tmp")
    .status();

// Capture output — output() defaults stdin to null, stdout and stderr to piped
auto out = Command{"/usr/bin/echo"}
    .arg("hello")
    .output();
// out->status, out->out (vector<byte>), out->err

// Full control via ChildProcess
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

`Stdio` factory methods: `null()`, `inherit()`, `piped()`, `from(IOResource)`.

**POSIX-only options:** `setSID()`, `arg0(name)`, `processGroup(pgid)`, `uid(uid)`, `gid(gid)`, `groups(gids)`, `preExec(f)`.

**Windows-only options:** `creationFlags(DWORD)`, `showWindow(WORD)`, `rawAttribute(attr, value, size)`.

### PseudoConsole (PTY)

```cpp
auto pty = PseudoConsole::make(/*rows=*/24, /*cols=*/80);
if (pty) {
    pty->resize(40, 120);
    auto child = pty->spawn(Command{"/bin/bash"});
    auto &master = pty->master(); // IOResource for reading/writing PTY
}
```

---

## Network Interfaces (`net.h`)

```cpp
auto ifaces = zero::os::net::interfaces(); // map<string, Interface> — throws

for (auto &[name, iface] : ifaces) {
    // iface.mac, iface.addresses (vector<Address>)
    for (auto &addr : iface.addresses) {
        // std::visit to handle IfAddress4 or IfAddress6
    }
}
```

IP type aliases: `IPv4 = std::array<std::byte, 4>`, `IPv6 = std::array<std::byte, 16>`, `IP = std::variant<IPv4, IPv6>`.

Constants: `LocalhostIPv4`, `BroadcastIPv4`, `UnspecifiedIPv4`, `LocalhostIPv6`, `UnspecifiedIPv6`.

`stringify(span<byte,4>)` → dotted-decimal; `stringify(span<byte,16>)` → colon-hex.

---

## System Statistics (`stat.h`)

```cpp
auto cpu = zero::os::stat::cpu();       // CPUTime{user, system, idle} — throws
auto mem = zero::os::stat::memory();    // MemoryStat{total, used, available, free, usedPercent} — throws
```

---

## Notes

- `self()`, `all()`, `hostname()`, `pipe()`, `interfaces()`, `cpu()`, `memory()` all throw on failure (they return the result type directly, not `expected`). Use them where failure is truly exceptional.
- `open(pid)` returns `expected` because targeting an arbitrary PID is a routine fallible operation.
- Process information methods on `Process` all return `expected` — platform APIs for these can fail (e.g., permission denied).
