# log — 日志

`#include <zero/log.h>`

命名空间：`zero::log`

---

## 概述

一个异步的多 Sink 日志系统，通过 `concurrent::channel` 将日志记录分发给后台线程处理。支持每个 Sink 独立的级别过滤、标签过滤和定期刷写。

---

## 主要类型

| 类型 | 作用 |
|---|---|
| `Level` | `Error`、`Warning`、`Info`、`Debug` |
| `Record` | `{level, line, filename, timestamp, content, optional<tag>}` |
| `ISink` | 接口：`write(record)`、`flush()` |
| `ConsoleSink` | 所有级别均写入 stderr |
| `FileSink` | 滚动文件 Sink |
| `Logger` | 多 Sink 异步日志器 |

---

## 快速配置

```cpp
#include <zero/log.h>

// 配置全局单例日志器为控制台输出
Z_INIT_CONSOLE_LOG(zero::log::Level::Debug)

// 或配置为滚动文件日志
Z_INIT_FILE_LOG(zero::log::Level::Info, "app", "logs/", /*maxSize=*/10*1024*1024, /*maxFiles=*/5)
```

这两个宏会配置 `zero::log::globalLogger()`。

---

## 日志宏

```cpp
Z_LOG_DEBUG("调试信息：{}", value);
Z_LOG_INFO("系统启动");
Z_LOG_WARNING("内存不足：{} MB", mb);
Z_LOG_ERROR("操作失败：{}", ec.message());

// 带标签的变体
Z_LOG_DEBUG_T("network", "已连接到 {}", host);
Z_LOG_INFO_T("auth", "用户 {} 已登录", username);
```

格式化使用 `fmt` 的 `{}` 占位符语法。

---

## 手动配置日志器

```cpp
auto logger = std::make_shared<zero::log::Logger>();

// 添加名为 "console" 的控制台 Sink，级别为 Info
logger->add(
    zero::log::Level::Info,
    std::make_unique<zero::log::ConsoleSink>(),
    "console"
);

// 添加带标签过滤的文件 Sink，每 1 秒刷写一次
logger->add(
    zero::log::Level::Debug,
    std::make_unique<zero::log::FileSink>("app", "logs/", 10*1024*1024, 5),
    "file-sink",
    {"network", "auth"},       // 可选标签过滤（空 = 接受所有标签）
    std::chrono::seconds{1}    // 刷写间隔
);

// 运行时调整级别
logger->setLevel("console", zero::log::Level::Warning);
logger->remove("console");

// 等待所有待处理记录写入完成
logger->sync();
```

---

## 级别与标签过滤

```cpp
if (logger->enabled(zero::log::Level::Debug)) { /* 跳过耗费资源的格式化 */ }
if (logger->enabled(zero::log::Level::Debug, "network")) { /* 标签范围的检查 */ }
```

---

## 记录格式

当被格式化时（如 `FileSink`），`Record` 渲染为：

```
2024-01-15 12:34:56 | INFO  |          main.cpp:42] 应用启动
```

已为 `Record` 提供 `fmt::formatter<Record>` 特化，可与 `fmt::format` 配合使用。

---

## 注意事项

- 日志器运行一个后台线程，通过 `concurrent::channel` 通信。调用 `sync()` 会阻塞直到所有排队的记录处理完毕。
- `globalLogger()` 返回 `Logger &` 引用。两个 `Z_INIT_*` 宏均向其添加一个 Sink（不会替换日志器本身）。
- 若未添加任何 Sink，日志调用会被静默丢弃。
- `FileSink` 在当前文件达到 `maxSize` 字节时执行滚动，最多保留 `maxFiles` 个历史文件。
