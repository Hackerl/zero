# utility — Defer、Expect 宏、单子辅助工具

头文件：
- `#include <zero/defer.h>` — 作用域退出守卫
- `#include <zero/expect.h>` — `Z_EXPECT`/`Z_TRY` 传播宏
- `#include <zero/utility.h>` — 单子辅助工具、`localTime`
- `#include <zero/formatter.h>` — `fmt::formatter<std::exception_ptr>`

---

## Defer (`defer.h`)

RAII 作用域退出守卫。回调会在 `Defer` 对象离开作用域时执行，即使发生异常也不例外。

```cpp
#include <zero/defer.h>

{
    auto fd = open("/tmp/file", O_RDONLY);
    Z_DEFER(close(fd)); // 在作用域结束时执行
}
```

`Z_DEFER(stmt)` 展开为 `zero::Defer deferN{[&]() { stmt; }}`，通过 `__COUNTER__` 保证每次展开使用唯一名称（如 `defer0`、`defer1`）。

---

## 错误传播宏 (`expect.h`)

### `Z_EXPECT(expr)`

若 `expr` 包含错误，则提前返回 `std::unexpected`；值需事后通过 `*expr` 访问。

```cpp
std::expected<int, std::error_code> readInt(IReader &r) {
    auto bytes = r.readExactly(4);
    Z_EXPECT(bytes);
    return /* 解析 *bytes */;
}
```

### `Z_CO_EXPECT(expr)`

协程变体——使用 `co_return std::unexpected{...}` 而非 `return`。

### `Z_TRY(expr)` *（仅 GCC/Clang）* / `Z_CO_TRY(expr)` *（仅 Clang）*

使用 GCC/Clang 语句表达式实现表达式级解包，可在更大的表达式中使用：

```cpp
int value = Z_TRY(someExpected); // 若未设置则返回错误，否则得到其值
```

---

## 单子辅助工具 (`utility.h`)

用于组合 `std::optional` 和 `std::expected` 的工具函数。

```cpp
#include <zero/utility.h>

// flatten：optional<optional<T>> → optional<T>
std::optional<std::optional<int>> nested = std::make_optional(std::make_optional(42));
auto flat = zero::flatten(nested); // optional<int>{42}

// flatten：expected<expected<T,E>,E> → expected<T,E>
auto flat2 = zero::flatten(nested_expected);

// extract：expected<T,E> → optional<T>（丢弃错误）
auto opt = zero::extract(some_expected); // optional<T>

// transpose：expected<optional<T>,E> → optional<expected<T,E>>
//        或：optional<expected<T,E>> → expected<optional<T>,E>
auto transposed = zero::transpose(opt_of_exp);

// flattenWith：展开并转换错误类型
auto flat3 = zero::flattenWith<NewError>(nested_expected);
```

---

## `localTime` (`utility.h`)

对 `localtime_r` / `localtime_s` 的线程安全封装：

```cpp
std::time_t t = std::time(nullptr);
std::tm tm = zero::localTime(t);
```

---

## `fmt::formatter<std::exception_ptr>` (`formatter.h`)

```cpp
#include <zero/formatter.h>
#include <fmt/format.h>

std::exception_ptr ptr = std::make_exception_ptr(std::runtime_error{"出错了"});
std::string s = fmt::format("{}", ptr);
// "exception(出错了)"

std::exception_ptr null;
std::string s2 = fmt::format("{}", null);
// "nullptr"
```

---

## 注意事项

- `Z_EXPECT` 要求 `expr` 是 `std::expected`（或具有 `.has_value()` 和 `.error()` 的兼容类型）。
- `Z_TRY` 使用 GCC/Clang 语句表达式，不支持 MSVC。`Z_CO_TRY` 仅限 Clang。
- `flatten`、`extract`、`transpose` 无内存分配，是对包装类型的纯变换操作。
