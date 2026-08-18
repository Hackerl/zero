# utility — Defer, Expect Macros, Monadic Helpers

Headers:
- `#include <zero/defer.h>` — scope-exit guard
- `#include <zero/expect.h>` — `Z_EXPECT`/`Z_TRY` propagation macros
- `#include <zero/utility.h>` — monadic helpers, `localTime`
- `#include <zero/formatter.h>` — `fmt::formatter<std::exception_ptr>`

---

## Defer (`defer.h`)

RAII scope-exit guard. The callback runs when the `Defer` object goes out of scope, even on exception.

```cpp
#include <zero/defer.h>

{
    auto fd = open("/tmp/file", O_RDONLY);
    Z_DEFER(close(fd)); // runs at end of scope
}
```

`Z_DEFER(stmt)` expands to `zero::Defer deferN{[&]() { stmt; }}` with a unique name per scope (using `__COUNTER__`).

---

## Error Propagation Macros (`expect.h`)

### `Z_EXPECT(expr)`

Early-return `std::unexpected` if `expr` holds an error. The value must be accessed separately via `*expr`.

```cpp
std::expected<int, std::error_code> readInt(IReader &r) {
    auto bytes = r.readExactly(4);
    Z_EXPECT(bytes);
    return /* parse *bytes */;
}
```

### `Z_CO_EXPECT(expr)`

Coroutine variant — uses `co_return std::unexpected{...}` instead of `return`.

### `Z_TRY(expr)` *(GCC/Clang only)* / `Z_CO_TRY(expr)` *(Clang only)*

Expression-level unwrap using GCC/Clang statement expressions. Usable inside a larger expression:

```cpp
int value = Z_TRY(someExpected); // returns error if not set, otherwise the value
```

---

## Monadic Helpers (`utility.h`)

Utilities for composing `std::optional` and `std::expected`.

```cpp
#include <zero/utility.h>

// flatten: optional<optional<T>> → optional<T>
std::optional<std::optional<int>> nested = std::make_optional(std::make_optional(42));
auto flat = zero::flatten(nested); // optional<int>{42}

// flatten: expected<expected<T,E>,E> → expected<T,E>
auto flat2 = zero::flatten(nested_expected);

// extract: expected<T,E> → optional<T>  (discards error)
auto opt = zero::extract(some_expected); // optional<T>

// transpose: expected<optional<T>,E> → optional<expected<T,E>>
//        or: optional<expected<T,E>> → expected<optional<T>,E>
auto transposed = zero::transpose(opt_of_exp);

// flattenWith: flatten with error type coercion
auto flat3 = zero::flattenWith<NewError>(nested_expected);
```

---

## `localTime` (`utility.h`)

Thread-safe wrapper around `localtime_r` / `localtime_s`:

```cpp
std::time_t t = std::time(nullptr);
std::tm tm = zero::localTime(t);
```

---

## `fmt::formatter<std::exception_ptr>` (`formatter.h`)

```cpp
#include <zero/formatter.h>
#include <fmt/format.h>

std::exception_ptr ptr = std::make_exception_ptr(std::runtime_error{"oops"});
std::string s = fmt::format("{}", ptr);
// "exception(oops)"

std::exception_ptr null;
std::string s2 = fmt::format("{}", null);
// "nullptr"
```

---

## Notes

- `Z_EXPECT` requires that `expr` is an `std::expected` (or compatible type with `.has_value()` and `.error()`).
- `Z_TRY` uses GCC/Clang statement expressions; not available on MSVC. `Z_CO_TRY` is Clang-only.
- `flatten`, `extract`, `transpose` do not allocate; they are pure transformations on the wrapper types.
