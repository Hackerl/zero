# atomic — Event & CircularBuffer

Headers:
- `#include <zero/atomic/event.h>`
- `#include <zero/atomic/circular_buffer.h>`

Namespace: `zero::atomic`

---

## Event

A manual-reset or auto-reset synchronization primitive backed by OS-native futex (`futex` on Linux, `ulock` on macOS, `WaitOnAddress` on Windows).

### Constructor

```cpp
Event(bool manual = false, bool initialState = false)
```

- `manual = false` — auto-reset: `wait()` resets the event before returning.
- `manual = true` — manual-reset: stays set until `reset()` is called.

### Methods

```cpp
// Wait until set (or timeout)
std::expected<void, std::error_code> wait(
    std::optional<std::chrono::milliseconds> timeout = std::nullopt
);

void set();    // signal all waiting threads
void reset();  // clear the event (for manual-reset mode)
bool isSet();  // query state without blocking
```

### Example

```cpp
zero::atomic::Event done{/*manual=*/true};

std::thread([&done] {
    // do work ...
    done.set();
}).detach();

auto result = done.wait(std::chrono::milliseconds{500});
if (!result) {
    // timed out or OS error
}
```

---

## CircularBuffer

A lock-free, multi-producer multi-consumer ring buffer based on a two-phase reserve/commit, acquire/release protocol.

### Template

```cpp
template<typename T>
class CircularBuffer;
```

### Construction

```cpp
zero::atomic::CircularBuffer<int> buf{/*capacity=*/64};
```

Capacity must be greater than 1.

### Producer Side

```cpp
// Reserve a slot (returns index on success)
std::optional<std::size_t> idx = buf.reserve();
if (idx) {
    buf[*idx] = 42;
    buf.commit(*idx);  // make visible to consumers
}
```

### Consumer Side

```cpp
std::optional<std::size_t> idx = buf.acquire();
if (idx) {
    int val = buf[*idx];
    buf.release(*idx);  // return slot to producers
}
```

### Queries

```cpp
buf.size();      // current number of committed items
buf.capacity();  // maximum capacity
buf.empty();
buf.full();
```

### Notes

- `reserve()` and `acquire()` return `std::nullopt` when the buffer is full or empty, respectively. They never block.
- Each slot must be committed after writing and released after reading. Skipping these calls corrupts the buffer state.
- `CircularBuffer` itself does not block. `concurrent::channel` wraps it with a mutex+condvar for the blocking `send()`/`receive()` paths.
