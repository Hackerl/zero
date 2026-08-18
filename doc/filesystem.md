# filesystem — Filesystem Wrappers

`#include <zero/filesystem.h>`

Namespace: `zero::filesystem`

---

## Overview

A thin, `std::expected`-based wrapper around `std::filesystem`. Every function that can fail returns `std::expected<T, std::error_code>` instead of throwing. The API mirrors `std::filesystem` naming conventions with camelCase names.

---

## Path Conversion

```cpp
// string_view → std::filesystem::path (UTF-8 on Windows)
auto path = zero::filesystem::path("/tmp/data.txt");

// path → UTF-8 string
std::string s = zero::filesystem::stringify(p);
```

---

## Application Paths

```cpp
auto exe = zero::filesystem::applicationPath();      // path (throws on failure)
auto dir = zero::filesystem::applicationDirectory(); // path (throws on failure)
```

---

## Reading and Writing Files

```cpp
// Read entire file as bytes
auto bytes = zero::filesystem::read("/etc/hosts");   // expected<vector<byte>, error_code>

// Read entire file as string
auto text = zero::filesystem::readString("/etc/hosts"); // expected<string, error_code>

// Write bytes
zero::filesystem::write("/tmp/out.bin", byte_span);  // expected<void, error_code>

// Write string
zero::filesystem::write("/tmp/out.txt", "hello\n");  // expected<void, error_code>
```

---

## Directory Iteration

```cpp
// Non-recursive
auto dir = zero::filesystem::readDirectory("/tmp");
if (dir) {
    while (true) {
        auto entry = dir->next(); // expected<optional<DirectoryEntry>, error_code>
        if (!entry || !*entry) break;
        auto &de = **entry;
        de.path();     // std::filesystem::path
        de.isDirectory();
        de.fileSize(); // expected<uintmax_t, error_code>
    }
}

// Recursive
auto walk = zero::filesystem::walkDirectory("/tmp");
```

`readDirectory` and `walkDirectory` accept optional `std::filesystem::directory_options`.

`DirectoryEntry` wraps `std::filesystem::directory_entry` with `expected`-returning methods: `fileSize()`, `lastWriteTime()`, `status()`, `symlinkStatus()`, `hardLinkCount()`, `isSymlink()`, `isRegularFile()`, `isDirectory()`, etc.

---

## Standard Filesystem Operations

All `std::filesystem` free functions are wrapped. A selection:

```cpp
zero::filesystem::exists(path)               // expected<bool, error_code>
zero::filesystem::createDirectory(path)      // expected<void, error_code>
zero::filesystem::createDirectories(path)    // expected<void, error_code>
zero::filesystem::remove(path)               // expected<void, error_code>
zero::filesystem::removeAll(path)            // expected<uintmax_t, error_code>
zero::filesystem::rename(from, to)           // expected<void, error_code>
zero::filesystem::copyFile(from, to)         // expected<void, error_code>
zero::filesystem::canonical(path)            // expected<path, error_code>
zero::filesystem::absolute(path)             // path (throws on failure)
zero::filesystem::fileSize(path)             // expected<uintmax_t, error_code>
zero::filesystem::currentPath()              // path (throws on failure)
zero::filesystem::temporaryDirectory()       // path (throws on failure)
zero::filesystem::space(path)               // expected<space_info, error_code>
zero::filesystem::createSymlink(to, link)    // expected<void, error_code>
zero::filesystem::readSymlink(path)          // expected<path, error_code>
```

---

## Behavioral differences from `std::filesystem`

These three functions diverge from their `std::filesystem` counterparts on edge cases:

| Function | `std::filesystem` return type | `zero::filesystem` return type | Edge-case difference |
|---|---|---|---|
| `createDirectory(path)` | `bool` — `true` if created, `false` if already existed | `expected<void, error_code>` | Already-exists → error (`EEXIST` / `ERROR_FILE_EXISTS`) instead of `false` |
| `remove(path)` | `bool` — `true` if removed, `false` if did not exist | `expected<void, error_code>` | Does-not-exist → error (`ENOENT` / `ERROR_FILE_NOT_FOUND`) instead of `false` |
| `removeAll(path)` | `uintmax_t` — count of removed entries, `0` if path did not exist | `expected<uintmax_t, error_code>` | Does-not-exist → error (`ENOENT` / `ERROR_FILE_NOT_FOUND`) instead of `0` |

`createDirectories` preserves the `std::filesystem::create_directories` behaviour and succeeds silently when the directory already exists.

---

## Notes

- `readDirectory` and `walkDirectory` return a `NoExcept<T>` iterator. Call `.next()` repeatedly; it returns `expected<optional<DirectoryEntry>>`. A `nullopt` optional signals end-of-directory.
- On Windows, path strings are UTF-8 encoded and converted to wide strings internally before being passed to the OS.
- All predicates (`isDirectory`, `isRegularFile`, `isSymlink`, etc.) are also available as free functions wrapping their `std::filesystem` equivalents.
