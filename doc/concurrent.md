# concurrent — Channel

`#include <zero/concurrent/channel.h>`

Namespace: `zero::concurrent`

---

## Overview

A bounded, thread-safe multi-producer multi-consumer (MPMC) channel backed by a lock-free `atomic::CircularBuffer`. Channels are closed automatically when the last `Sender` or `Receiver` is destroyed.

---

## Key Types

| Type | Role |
|---|---|
| `Sender<T>` | Reference-counted send endpoint; copyable |
| `Receiver<T>` | Reference-counted receive endpoint; copyable |
| `Channel<T>` | `std::pair<Sender<T>, Receiver<T>>` |
| `TrySendError` | `Disconnected`, `Full` |
| `SendError` | `Disconnected`, `Timeout` |
| `TryReceiveError` | `Disconnected`, `Empty` |
| `ReceiveError` | `Disconnected`, `Timeout` |
| `ChannelError` | Error condition matching all `Disconnected` variants |

---

## Creating a Channel

```cpp
auto [sender, receiver] = zero::concurrent::channel<int>(/*capacity=*/16);
```

The internal buffer holds `capacity` elements. Sending blocks when full; receiving blocks when empty.

---

## Sending

```cpp
// Blocking send (no timeout)
auto result = sender.send(42);
if (!result) {
    if (result.error() == SendError::Disconnected) { /* receiver dropped */ }
}

// Blocking send with timeout
using namespace std::chrono_literals;
auto result = sender.send(42, 100ms);
if (!result && result.error() == SendError::Timeout) { /* timed out */ }

// Non-blocking attempt
auto result = sender.trySend(42);
if (!result) {
    if (result.error() == TrySendError::Full) { /* buffer full */ }
}
```

`sendEx` / `trySendEx` return the element back inside the error so it can be reclaimed on failure.

---

## Receiving

```cpp
// Blocking receive (no timeout)
auto val = receiver.receive();
if (!val) {
    if (val.error() == ReceiveError::Disconnected) { /* sender dropped and buffer empty */ }
}
// use *val

// Blocking receive with timeout
auto val = receiver.receive(50ms);

// Non-blocking
auto val = receiver.tryReceive();
if (!val && val.error() == TryReceiveError::Empty) { /* nothing yet */ }
```

---

## Channel Lifecycle

```cpp
{
    auto [s, r] = channel<int>(4);
    s.send(1);
    // When s goes out of scope, the channel closes.
    // r.receive() will drain remaining items, then return Disconnected.
}

// Explicit close
sender.close();
```

---

## Error Conditions

`ChannelError::Disconnected` is an `std::error_condition` that matches all four `Disconnected` error codes:

```cpp
if (result.error() == make_error_condition(ChannelError::Disconnected)) {
    // covers TrySendError::Disconnected, SendError::Disconnected, etc.
}
```

---

## Notes

- `Sender` and `Receiver` are both copyable. Each copy increments a reference count. The channel closes when the last `Sender` or the last `Receiver` is destroyed.
- `receive()` / `send()` with no timeout block indefinitely.
- The buffer is implemented as a lock-free ring; the mutex is only acquired when a thread needs to wait (blocking path).
- Thread-safe: multiple producers and multiple consumers are supported simultaneously.
