# async — Promise/Future

`#include <zero/async/promise.h>`

Namespace: `zero::async::promise`

---

## Overview

A header-only, composable async primitive built on top of `std::expected` and `zero::atomic::Event`. It provides producer-side `Promise<T, E>` and consumer-side `Future<T, E>` / `SemiFuture<T, E>` types, supporting chaining, error recovery, and aggregation across threads.

The default error type `E` is `std::exception_ptr`. For typed errors, specify a concrete type (e.g., `Promise<int, std::error_code>`).

---

## Key Types

| Type | Role |
|---|---|
| `Promise<T, E>` | Producer — call `resolve()` / `reject()` to settle |
| `SemiFuture<T, E>` | Future without a bound executor; call `.via(executor)` to get a `Future` |
| `Future<T, E>` | Consumer — chainable with `.then()`, `.fail()`, `.finally()` |
| `Contract<T, E>` | `std::pair<Promise<T,E>, Future<T,E>>` — result of `contract(executor)` |
| `IExecutor` | Callback dispatch interface |
| `InlineExecutor` | Calls callbacks synchronously on the settling thread |

---

## Creating Futures

```cpp
// Already-resolved / rejected
auto f1 = Future<int>::resolved(42);
auto f2 = Future<int, std::error_code>::rejected(std::make_error_code(std::errc::invalid_argument));

// Producer/consumer pair
auto [promise, future] = contract<std::string>(InlineExecutor::instance());

std::thread([p = std::move(promise)]() mutable {
    p.resolve("hello");
}).detach();

// future is a Future<std::string> ready to chain
```

Return `resolve(value)` / `reject(error)` from a `.then()` / `.fail()` callback to create an already-settled future without constructing a `Promise` manually — they implicitly convert to `SemiFuture` or `Future`.

---

## Chaining

```cpp
Future<int>::resolved(10)
    .then([](int v) { return v * 2; })              // plain callback → new value
    .then([](int v) -> std::expected<int, std::error_code> { // fallible callback
        if (v > 10) return v;
        return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
    })
    .fail([](std::error_code ec) { return 0; })     // recover from error
    .finally([] { /* always runs */ });
```

Callback overload resolution for `.then(f)`:
- `f` returns `Future<U, E>` → async continuation
- `f` returns `std::expected<U, E>` → fallible step (non-`exception_ptr` E only)
- `f` returns anything else → plain transform (exceptions captured when `E = std::exception_ptr`)

`.then(f1, f2)` — run `f1` on success, `f2` on error, but `f2` does **not** intercept errors thrown inside `f1`.

---

## Blocking Get

```cpp
// E = std::exception_ptr: throws on error
auto val = std::move(future).get();

// E = std::error_code: returns expected<T, E>
auto result = std::move(future).get();

// wait with timeout
auto ok = future.wait(std::chrono::milliseconds{100});
```

---

## Aggregation

All aggregation functions take either variadic `Future` arguments or an iterator/range.

```cpp
auto f1 = Future<int>::resolved(1);
auto f2 = Future<int>::resolved(2);

// all — resolves with all values; rejects on first failure
all(std::move(f1), std::move(f2))  // → SemiFuture<std::array<int,2>>

// any — resolves with the first success; rejects if all fail
any(std::move(f1), std::move(f2))  // → SemiFuture<int, std::array<E,2>>

// race — settles with the first to settle (success or failure)
race(std::move(f1), std::move(f2)) // → SemiFuture<int>

// allSettled — always resolves; result contains each future's outcome
allSettled(std::move(f1), std::move(f2)) // → SemiFuture<std::array<expected<int,E>,2>>
```

Range overloads also exist for homogeneous collections of `Future<T, E>`.

The aggregation functions require `Future` objects (with a bound executor). If you have `SemiFuture`, call `.via(executor)` first.

---

## Building a Thread Pool

The test suite demonstrates building a thread pool on top of `concurrent::Channel` and `IExecutor`:

```cpp
class ThreadPool : public IExecutor {
    zero::concurrent::Sender<std::function<void()>> mSender;
    std::vector<std::thread> mWorkers;
public:
    explicit ThreadPool(int n) {
        auto [sender, receiver] = zero::concurrent::channel<std::function<void()>>(64);
        mSender = std::move(sender);
        for (int i = 0; i < n; ++i)
            mWorkers.emplace_back([r = receiver]() mutable {
                while (auto task = r.receive())
                    (*task)();
            });
    }
    void post(std::function<void()> f) override { mSender.send(std::move(f)); }
};
```

---

## Notes

- Each `Future` / `SemiFuture` is move-only and can hold at most one callback.
- `SemiFuture` has no executor — it cannot be directly chained. Call `.via(executor)` first.
- `InlineExecutor` runs callbacks synchronously on the settling thread. For multi-threaded use, supply a custom executor.
- Calling `all()`, `any()`, `race()`, or `allSettled()` on an empty range throws `std::invalid_argument`.
