# error — 错误处理

`#include <zero/error.h>`

命名空间：`zero::error`

---

## 概述

本模块提供：
1. 用于定义 `std::error_code` 枚举及其关联 `std::error_category` 子类的**宏系统**，无需手写样板代码。
2. 使用 placement new 的**单例工具**（`categoryImmortalize`/`categoryInstance`），避免静态析构顺序问题和 Windows UCRT `atexit` 死锁。
3. **异常桥接**：`guard()`、`capture()`、`StacktraceError<T>`。

---

## 定义错误码

### 基本用法（无 default_error_condition 映射）

```cpp
// 在头文件中：
Z_DEFINE_ERROR_CODE(
    MyError,                            // 枚举名
    "mylib::operation",                 // 分类名字符串
    NotFound, "资源未找到",
    TimedOut, "操作超时"
)
Z_DECLARE_ERROR_CODE(MyError)           // 特化 std::is_error_code_enum
Z_DEFINE_ERROR_CATEGORY_INSTANCE(MyError) // 在某一个 .cpp 文件中

// 使用：
std::error_code ec = make_error_code(MyError::NotFound);
ec.message(); // "资源未找到"
```

### 带 default_error_condition 映射

```cpp
Z_DEFINE_ERROR_CODE_EX(
    MyError,
    "mylib::operation",
    NotFound,  "未找到",  std::errc::no_such_file_or_directory,
    TimedOut,  "超时",    std::errc::timed_out
)
Z_DECLARE_ERROR_CODE(MyError)
Z_DEFINE_ERROR_CATEGORY_INSTANCE(MyError)
```

在第三列使用 `Z_DEFAULT_ERROR_CONDITION` 可委托给基类 `error_category::default_error_condition`。

### 错误条件（Error Condition）

当需要跨分类匹配枚举时，使用 `Z_DEFINE_ERROR_CONDITION` / `Z_DEFINE_ERROR_CONDITION_EX`。`EX` 变体传入一个 lambda，用于检查哪些错误码匹配：

```cpp
Z_DEFINE_ERROR_CONDITION_EX(
    ChannelError,
    "mylib::channel",
    Disconnected, "Channel 已断开",
    [](const std::error_code &ec) {
        return ec == make_error_code(SenderError::Disconnected) ||
               ec == make_error_code(ReceiverError::Disconnected);
    }
)
Z_DECLARE_ERROR_CONDITION(ChannelError)
Z_DEFINE_ERROR_CATEGORY_INSTANCE(ChannelError)
```

### 错误转换器（Error Transformer）

用于封装 C API 的整型错误码并提供自定义的字符串化函数：

```cpp
Z_DEFINE_ERROR_TRANSFORMER(MyAPIError, "myapi", [](int v) -> std::string {
    return myapi_strerror(v);
})
```

### 类内（Inner）变体

在宏名后加 `_INNER` 可在类内部定义枚举（`make_error_code` 使用 `friend` 而非自由函数）：

```cpp
class Foo {
    Z_DEFINE_ERROR_CODE_INNER(BarError, "foo::bar", A, "a", B, "b")
};
```

---

## 异常桥接

### `guard(expected)` — 失败时抛异常

```cpp
std::expected<int, std::error_code> result = someOp();
int value = zero::error::guard(std::move(result)); // 出错时抛 StacktraceError<std::system_error>
```

### `capture(f)` — 将抛出的异常转换为 `unexpected`

```cpp
auto result = zero::error::capture([&] {
    return riskyOperation(); // 可能抛出异常
});
// result 为 std::expected<T, std::exception_ptr>
```

### `StacktraceError<T>` — 带栈追踪的异常

```cpp
throw zero::error::StacktraceError<std::runtime_error>{"出现问题"};
```

当 `<stacktrace>` 可用时（`__cpp_lib_stacktrace >= 202011L`），会附带栈追踪信息；否则退化为普通 `T`。

---

## 单例安全性

`categoryImmortalize<T>()` 使用 placement new 将 `T` 构造到静态 `std::array<std::byte, sizeof(T)>` 中，使其析构函数永远不会运行。这可以避免：
- 当 `std::error_code` 值的生命周期超过其分类对象时出现的静态析构顺序问题。
- 在非主线程的析构函数中访问 `std::error_category` 导致的 Windows UCRT `atexit` 死锁。

所有通过 `Z_DEFINE_ERROR_CATEGORY_INSTANCE` 定义的错误分类都自动使用此模式。

---

## 注意事项

- `Z_DEFINE_ERROR_CATEGORY_INSTANCE(T)` 必须只出现在一个 `.cpp` 文件中。
- `Z_DECLARE_ERROR_CODE(T)` / `Z_DECLARE_ERROR_CONDITION(T)` 必须放在定义枚举的头文件中，以便在使用前特化 `std::is_error_code_enum`。
- `Z_DECLARE_ERROR_CODES(A, B, C)` 是同时声明多个错误码的快捷写法。
