# async — Promise/Future

`#include <zero/async/promise.h>`

命名空间：`zero::async::promise`

---

## 概述

一个纯头文件实现的可组合异步原语，基于 `std::expected` 和 `zero::atomic::Event`。提供生产者侧 `Promise<T, E>` 和消费者侧 `Future<T, E>` / `SemiFuture<T, E>` 类型，支持链式调用、错误恢复和跨线程聚合。

默认错误类型 `E` 为 `std::exception_ptr`。如需类型化错误，可指定具体类型（如 `Promise<int, std::error_code>`）。

---

## 主要类型

| 类型 | 作用 |
|---|---|
| `Promise<T, E>` | 生产者——调用 `resolve()` / `reject()` 来完成 |
| `SemiFuture<T, E>` | 未绑定执行器的 Future；调用 `.via(executor)` 获得 `Future` |
| `Future<T, E>` | 消费者——支持 `.then()`、`.fail()`、`.finally()` 链式调用 |
| `Contract<T, E>` | `std::pair<Promise<T,E>, Future<T,E>>`——`contract(executor)` 的返回值 |
| `IExecutor` | 回调调度接口 |
| `InlineExecutor` | 在完成线程上同步调用回调 |

---

## 创建 Future

```cpp
// 已完成 / 已拒绝的 Future
auto f1 = Future<int>::resolved(42);
auto f2 = Future<int, std::error_code>::rejected(std::make_error_code(std::errc::invalid_argument));

// 生产者/消费者对
auto [promise, future] = contract<std::string>(InlineExecutor::instance());

std::thread([p = std::move(promise)]() mutable {
    p.resolve("hello");
}).detach();
```

在 `.then()` / `.fail()` 回调中返回 `resolve(value)` / `reject(error)` 可创建已完成的 Future，无需手动构造 `Promise`——它们可隐式转换为 `SemiFuture` 或 `Future`。

---

## 链式调用

```cpp
Future<int>::resolved(10)
    .then([](int v) { return v * 2; })              // 普通回调 → 新值
    .then([](int v) -> std::expected<int, std::error_code> { // 可失败回调
        if (v > 10) return v;
        return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
    })
    .fail([](std::error_code ec) { return 0; })     // 从错误中恢复
    .finally([] { /* 始终执行 */ });
```

`.then(f)` 的重载解析：
- `f` 返回 `Future<U, E>` → 异步后续
- `f` 返回 `std::expected<U, E>` → 可失败步骤（仅限非 `exception_ptr` 的 E）
- 其他返回值 → 普通变换（当 `E = std::exception_ptr` 时捕获异常）

`.then(f1, f2)` — 成功时运行 `f1`，失败时运行 `f2`，但 `f2` 不拦截 `f1` 内抛出的异常。

---

## 阻塞获取

```cpp
// E = std::exception_ptr：出错时抛异常
auto val = std::move(future).get();

// E = std::error_code：返回 expected<T, E>
auto result = std::move(future).get();

// 带超时的等待
auto ok = future.wait(std::chrono::milliseconds{100});
```

---

## 聚合操作

所有聚合函数既接受可变参数 `Future`，也接受迭代器/范围。

```cpp
auto f1 = Future<int>::resolved(1);
auto f2 = Future<int>::resolved(2);

// all — 全部成功则完成；任一失败则拒绝
all(std::move(f1), std::move(f2))      // → SemiFuture<std::array<int,2>>

// any — 任一成功则完成；全部失败则拒绝
any(std::move(f1), std::move(f2))      // → SemiFuture<int, std::array<E,2>>

// race — 以最先完成的 Future 的结果（成功或失败）为准
race(std::move(f1), std::move(f2))     // → SemiFuture<int>

// allSettled — 始终完成；结果包含每个 Future 的状态
allSettled(std::move(f1), std::move(f2)) // → SemiFuture<std::array<expected<int,E>,2>>
```

---

## 注意事项

- 每个 `Future` / `SemiFuture` 仅支持移动，且最多只能注册一个回调。
- `SemiFuture` 没有执行器，不能直接链式调用。需先调用 `.via(executor)`。
- `InlineExecutor` 在完成线程上同步运行回调。多线程场景请提供自定义执行器。
- 对空范围调用 `all()`、`any()`、`race()` 或 `allSettled()` 会抛出 `std::invalid_argument`。
