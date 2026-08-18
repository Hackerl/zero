# atomic — Event 与 CircularBuffer

头文件：
- `#include <zero/atomic/event.h>`
- `#include <zero/atomic/circular_buffer.h>`

命名空间：`zero::atomic`

---

## Event

一个手动复位或自动复位的同步原语，底层使用系统原生的 futex（Linux 上为 `futex`，macOS 上为 `ulock`，Windows 上为 `WaitOnAddress`）。

### 构造函数

```cpp
Event(bool manual = false, bool initialState = false)
```

- `manual = false` — 自动复位：`wait()` 返回前自动重置事件。
- `manual = true` — 手动复位：保持已触发状态，直到显式调用 `reset()`。

### 方法

```cpp
// 等待直到被触发（或超时）
std::expected<void, std::error_code> wait(
    std::optional<std::chrono::milliseconds> timeout = std::nullopt
);

void set();    // 唤醒所有等待线程
void reset();  // 清除事件（用于手动复位模式）
bool isSet();  // 不阻塞地查询状态
```

### 示例

```cpp
zero::atomic::Event done{/*manual=*/true};

std::thread([&done] {
    // 执行工作 ...
    done.set();
}).detach();

auto result = done.wait(std::chrono::milliseconds{500});
if (!result) {
    // 超时或 OS 错误
}
```

---

## CircularBuffer

一个无锁的多生产者多消费者环形缓冲区，基于两阶段的 reserve/commit（生产者侧）和 acquire/release（消费者侧）协议。

### 模板

```cpp
template<typename T>
class CircularBuffer;
```

### 构造

```cpp
zero::atomic::CircularBuffer<int> buf{/*capacity=*/64};
```

容量必须大于 1。

### 生产者端

```cpp
// 预留一个槽位（返回槽位索引）
std::optional<std::size_t> idx = buf.reserve();
if (idx) {
    buf[*idx] = 42;
    buf.commit(*idx);  // 使其对消费者可见
}
```

### 消费者端

```cpp
std::optional<std::size_t> idx = buf.acquire();
if (idx) {
    int val = buf[*idx];
    buf.release(*idx);  // 将槽位归还给生产者
}
```

### 查询方法

```cpp
buf.size();      // 已提交的元素数量
buf.capacity();  // 最大容量
buf.empty();
buf.full();
```

### 注意事项

- `reserve()` 和 `acquire()` 在缓冲区已满或为空时返回 `std::nullopt`，从不阻塞。
- 每个槽位写入后必须调用 `commit()`，读取后必须调用 `release()`。跳过这些调用会破坏缓冲区状态。
- `CircularBuffer` 本身不会阻塞。`concurrent::channel` 在其之上封装了互斥锁和条件变量，用于实现阻塞的 `send()`/`receive()` 路径。
