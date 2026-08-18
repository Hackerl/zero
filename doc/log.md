# log — Logging

`#include <zero/log.h>`

Namespace: `zero::log`

---

## Overview

An asynchronous, multi-sink logger that dispatches log records through a `concurrent::channel` to a background thread. Supports per-sink level filtering, tag filtering, and periodic flushing.

---

## Key Types

| Type | Role |
|---|---|
| `Level` | `Error`, `Warning`, `Info`, `Debug` |
| `Record` | `{level, line, filename, timestamp, content, optional<tag>}` |
| `ISink` | Interface: `write(record)`, `flush()` |
| `ConsoleSink` | Writes all levels to stderr |
| `FileSink` | Rotating file sink |
| `Logger` | Multi-sink async logger |

---

## Quick Setup

```cpp
#include <zero/log.h>

// Initialize a console logger at the global singleton
Z_INIT_CONSOLE_LOG(zero::log::Level::Debug)

// Or a rotating file logger
Z_INIT_FILE_LOG(zero::log::Level::Info, "app", "logs/", /*maxSize=*/10*1024*1024, /*maxFiles=*/5)
```

These macros configure `zero::log::globalLogger()`.

---

## Logging Macros

```cpp
Z_LOG_DEBUG("debug message: {}", value);
Z_LOG_INFO("starting up");
Z_LOG_WARNING("low memory: {} MB", mb);
Z_LOG_ERROR("failed: {}", ec.message());

// Tagged variants
Z_LOG_DEBUG_T("network", "connected to {}", host);
Z_LOG_INFO_T("auth", "user {} logged in", username);
```

The `{}`-style formatting uses `fmt`.

---

## Manual Logger Setup

```cpp
auto logger = std::make_shared<zero::log::Logger>();

// Add a console sink named "console" at Info level
logger->add(
    zero::log::Level::Info,
    std::make_unique<zero::log::ConsoleSink>(),
    "console"
);

// Add a file sink with tag filtering and flush every 1 second
logger->add(
    zero::log::Level::Debug,
    std::make_unique<zero::log::FileSink>("app", "logs/", 10*1024*1024, 5),
    "file-sink",
    {"network", "auth"},       // optional tag filter (empty = accept all)
    std::chrono::seconds{1}    // flush interval
);

// Adjust level at runtime
logger->setLevel("console", zero::log::Level::Warning);
logger->remove("console");

// Wait for all pending records to be written
logger->sync();
```

---

## Level and Tag Filtering

```cpp
if (logger->enabled(zero::log::Level::Debug)) { /* skip expensive formatting */ }
if (logger->enabled(zero::log::Level::Debug, "network")) { /* tag-scoped check */ }
```

---

## Record Format

When formatted (e.g., for `FileSink`), a `Record` is rendered as:

```
2024-01-15 12:34:56 | INFO  |          main.cpp:42] Starting application
```

`fmt::formatter<Record>` is provided for use with `fmt::format`.

---

## Notes

- The logger runs a background thread and communicates via `concurrent::channel`. Calling `sync()` blocks until all queued records are processed.
- `globalLogger()` returns a `Logger &` reference. Both `Z_INIT_*` macros add a sink to it (they do not replace it).
- If no sinks are added, log calls are silently dropped.
- `FileSink` rotates when the current file reaches `maxSize` bytes. It keeps at most `maxFiles` rotated files.
