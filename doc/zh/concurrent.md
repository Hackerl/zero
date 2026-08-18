# concurrent — Channel

`#include <zero/concurrent/channel.h>`

命名空间：`zero::concurrent`

---

## 概述

一个有界的线程安全多生产者多消费者（MPMC）Channel，底层基于无锁 `atomic::CircularBuffer`。当最后一个 `Sender` 或 `Receiver` 被销毁时，Channel 自动关闭。

---

## 主要类型

| 类型 | 作用 |
|---|---|
| `Sender<T>` | 引用计数的发送端；可拷贝 |
| `Receiver<T>` | 引用计数的接收端；可拷贝 |
| `Channel<T>` | `std::pair<Sender<T>, Receiver<T>>` |
| `TrySendError` | `Disconnected`（已断开）、`Full`（已满） |
| `SendError` | `Disconnected`、`Timeout`（超时） |
| `TryReceiveError` | `Disconnected`、`Empty`（为空） |
| `ReceiveError` | `Disconnected`、`Timeout` |
| `ChannelError` | 匹配所有 `Disconnected` 变体的错误条件 |

---

## 创建 Channel

```cpp
auto [sender, receiver] = zero::concurrent::channel<int>(/*capacity=*/16);
```

内部缓冲区可容纳 `capacity` 个元素。缓冲区满时发送阻塞，缓冲区空时接收阻塞。

---

## 发送

```cpp
// 阻塞发送（无超时）
auto result = sender.send(42);
if (!result) {
    if (result.error() == SendError::Disconnected) { /* 接收端已销毁 */ }
}

// 带超时的阻塞发送
using namespace std::chrono_literals;
auto result = sender.send(42, 100ms);
if (!result && result.error() == SendError::Timeout) { /* 超时 */ }

// 非阻塞尝试发送
auto result = sender.trySend(42);
if (!result) {
    if (result.error() == TrySendError::Full) { /* 缓冲区已满 */ }
}
```

`sendEx` / `trySendEx` 在失败时将元素放回错误值中，便于恢复。

---

## 接收

```cpp
// 阻塞接收（无超时）
auto val = receiver.receive();
if (!val) {
    if (val.error() == ReceiveError::Disconnected) { /* 发送端已销毁且缓冲区为空 */ }
}
// 使用 *val

// 带超时的阻塞接收
auto val = receiver.receive(50ms);

// 非阻塞
auto val = receiver.tryReceive();
if (!val && val.error() == TryReceiveError::Empty) { /* 暂无数据 */ }
```

---

## Channel 生命周期

```cpp
{
    auto [s, r] = channel<int>(4);
    s.send(1);
    // 当 s 离开作用域时，Channel 关闭。
    // r.receive() 会先排空剩余元素，然后返回 Disconnected。
}

// 显式关闭
sender.close();
```

---

## 错误条件

`ChannelError::Disconnected` 是一个 `std::error_condition`，可匹配所有四种 `Disconnected` 错误码：

```cpp
if (result.error() == make_error_condition(ChannelError::Disconnected)) {
    // 涵盖 TrySendError::Disconnected、SendError::Disconnected 等
}
```

---

## 注意事项

- `Sender` 和 `Receiver` 均可拷贝，每次拷贝增加引用计数。当最后一个 `Sender` 或最后一个 `Receiver` 被销毁时，Channel 关闭。
- 无超时的 `receive()` / `send()` 会无限阻塞。
- 缓冲区采用无锁环形结构；互斥锁仅在线程需要等待时（阻塞路径）才会被获取。
- 线程安全：支持同时有多个生产者和多个消费者。
