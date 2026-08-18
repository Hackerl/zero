# error — Error Handling

`#include <zero/error.h>`

Namespace: `zero::error`

---

## Overview

This module provides:
1. A **macro system** for defining `std::error_code` enums and their associated `std::error_category` subclasses without boilerplate.
2. **Singleton utilities** (`categoryImmortalize`/`categoryInstance`) that use placement new to avoid static destructor ordering issues and Windows UCRT `atexit` deadlocks.
3. **Exception bridges**: `guard()`, `capture()`, `StacktraceError<T>`.

---

## Defining Error Codes

### Basic (no default_error_condition mapping)

```cpp
// In a header:
Z_DEFINE_ERROR_CODE(
    MyError,                            // enum name
    "mylib::operation",                 // category name string
    NotFound, "Resource not found",
    TimedOut, "Operation timed out"
)
Z_DECLARE_ERROR_CODE(MyError)           // specializes std::is_error_code_enum
Z_DEFINE_ERROR_CATEGORY_INSTANCE(MyError) // in one .cpp file

// Usage:
std::error_code ec = make_error_code(MyError::NotFound);
ec.message(); // "Resource not found"
```

### With default_error_condition mapping

```cpp
Z_DEFINE_ERROR_CODE_EX(
    MyError,
    "mylib::operation",
    NotFound,  "Not found",  std::errc::no_such_file_or_directory,
    TimedOut,  "Timed out",  std::errc::timed_out
)
Z_DECLARE_ERROR_CODE(MyError)
Z_DEFINE_ERROR_CATEGORY_INSTANCE(MyError)
```

`Z_DEFAULT_ERROR_CONDITION` in the third column delegates to the base `error_category::default_error_condition`.

### Error Conditions

Use `Z_DEFINE_ERROR_CONDITION` / `Z_DEFINE_ERROR_CONDITION_EX` when you want a cross-category matching enum. Pass a lambda for `EX` that checks which error codes match:

```cpp
Z_DEFINE_ERROR_CONDITION_EX(
    ChannelError,
    "mylib::channel",
    Disconnected, "Channel disconnected",
    [](const std::error_code &ec) {
        return ec == make_error_code(SenderError::Disconnected) ||
               ec == make_error_code(ReceiverError::Disconnected);
    }
)
Z_DECLARE_ERROR_CONDITION(ChannelError)
Z_DEFINE_ERROR_CATEGORY_INSTANCE(ChannelError)
```

### Error Transformers

For wrapping integer error codes from C APIs with a custom stringify function:

```cpp
Z_DEFINE_ERROR_TRANSFORMER(MyAPIError, "myapi", [](int v) -> std::string {
    return myapi_strerror(v);
})
```

### Inner (Class-Scoped) Variants

Append `_INNER` to any macro to define the enum inside a class (uses `friend` instead of a free function for `make_error_code`):

```cpp
class Foo {
    Z_DEFINE_ERROR_CODE_INNER(BarError, "foo::bar", A, "a", B, "b")
};
```

---

## Exception Bridges

### `guard(expected)` — throw on failure

```cpp
std::expected<int, std::error_code> result = someOp();
int value = zero::error::guard(std::move(result)); // throws StacktraceError<std::system_error> on error
```

### `capture(f)` — convert thrown exceptions to `unexpected`

```cpp
auto result = zero::error::capture([&] {
    return riskyOperation(); // may throw
});
// result is std::expected<T, std::exception_ptr>
```

### `StacktraceError<T>` — exception with stacktrace

```cpp
throw zero::error::StacktraceError<std::runtime_error>{"something went wrong"};
```

When `<stacktrace>` is available (`__cpp_lib_stacktrace >= 202011L`), the stacktrace is attached. Otherwise it falls back to a plain `T`.

---

## Singleton Safety

`categoryImmortalize<T>()` constructs `T` with placement new into a static `std::array<std::byte, sizeof(T)>`, preventing the destructor from ever running. This sidesteps:
- Static destruction order fiasco when `std::error_code` values outlive their category.
- Windows UCRT `atexit` deadlock when a destructor accesses `std::error_category` from a non-main thread.

Every error category defined with `Z_DEFINE_ERROR_CATEGORY_INSTANCE` uses this pattern automatically.

---

## Notes

- `Z_DEFINE_ERROR_CATEGORY_INSTANCE(T)` must appear in exactly one `.cpp` file.
- `Z_DECLARE_ERROR_CODE(T)` / `Z_DECLARE_ERROR_CONDITION(T)` must be in the header that defines the enum so `std::is_error_code_enum` is specialized before use.
- `Z_DECLARE_ERROR_CODES(A, B, C)` is a convenience shorthand for declaring multiple codes at once.
