# filesystem — 文件系统封装

`#include <zero/filesystem.h>`

命名空间：`zero::filesystem`

---

## 概述

对 `std::filesystem` 的轻量封装，基于 `std::expected`。所有可能失败的函数均返回 `std::expected<T, std::error_code>`，而非抛出异常。API 遵循 `std::filesystem` 的命名约定，使用驼峰命名。

---

## 路径转换

```cpp
// string_view → std::filesystem::path（Windows 上为 UTF-8）
auto path = zero::filesystem::path("/tmp/data.txt");

// path → UTF-8 字符串
std::string s = zero::filesystem::stringify(p);
```

---

## 应用程序路径

```cpp
auto exe = zero::filesystem::applicationPath();      // path（失败时抛异常）
auto dir = zero::filesystem::applicationDirectory(); // path（失败时抛异常）
```

---

## 文件读写

```cpp
// 将整个文件读为字节
auto bytes = zero::filesystem::read("/etc/hosts");      // expected<vector<byte>, error_code>

// 将整个文件读为字符串
auto text = zero::filesystem::readString("/etc/hosts"); // expected<string, error_code>

// 写入字节
zero::filesystem::write("/tmp/out.bin", byte_span);  // expected<void, error_code>

// 写入字符串
zero::filesystem::write("/tmp/out.txt", "hello\n");  // expected<void, error_code>
```

---

## 目录遍历

```cpp
// 非递归遍历
auto dir = zero::filesystem::readDirectory("/tmp");
if (dir) {
    while (true) {
        auto entry = dir->next(); // expected<optional<DirectoryEntry>, error_code>
        if (!entry || !*entry) break;
        auto &de = **entry;
        de.path();        // std::filesystem::path
        de.isDirectory();
        de.fileSize();    // expected<uintmax_t, error_code>
    }
}

// 递归遍历
auto walk = zero::filesystem::walkDirectory("/tmp");
```

`readDirectory` 和 `walkDirectory` 可接受可选的 `std::filesystem::directory_options`。

`DirectoryEntry` 封装了 `std::filesystem::directory_entry`，其方法均返回 `expected`：`fileSize()`、`lastWriteTime()`、`status()`、`symlinkStatus()`、`hardLinkCount()`、`isSymlink()`、`isRegularFile()`、`isDirectory()` 等。

---

## 标准文件系统操作

所有 `std::filesystem` 自由函数均已封装。部分示例：

```cpp
zero::filesystem::exists(path)               // expected<bool, error_code>
zero::filesystem::createDirectory(path)      // expected<void, error_code>
zero::filesystem::createDirectories(path)    // expected<void, error_code>
zero::filesystem::remove(path)               // expected<void, error_code>
zero::filesystem::removeAll(path)            // expected<uintmax_t, error_code>
zero::filesystem::rename(from, to)           // expected<void, error_code>
zero::filesystem::copyFile(from, to)         // expected<void, error_code>
zero::filesystem::canonical(path)            // expected<path, error_code>
zero::filesystem::absolute(path)             // path（失败时抛异常）
zero::filesystem::fileSize(path)             // expected<uintmax_t, error_code>
zero::filesystem::currentPath()              // path（失败时抛异常）
zero::filesystem::temporaryDirectory()       // path（失败时抛异常）
zero::filesystem::space(path)               // expected<space_info, error_code>
zero::filesystem::createSymlink(to, link)    // expected<void, error_code>
zero::filesystem::readSymlink(path)          // expected<path, error_code>
```

---

## 与 `std::filesystem` 的行为差异

以下三个函数在边界情况下与 `std::filesystem` 对应函数的行为不同：

| 函数 | `std::filesystem` 返回类型 | `zero::filesystem` 返回类型 | 边界情况差异 |
|---|---|---|---|
| `createDirectory(path)` | `bool`——创建成功为 `true`，已存在为 `false` | `expected<void, error_code>` | 已存在 → 返回错误（`EEXIST` / `ERROR_FILE_EXISTS`），而非 `false` |
| `remove(path)` | `bool`——删除成功为 `true`，不存在为 `false` | `expected<void, error_code>` | 不存在 → 返回错误（`ENOENT` / `ERROR_FILE_NOT_FOUND`），而非 `false` |
| `removeAll(path)` | `uintmax_t`——已删除条目数，不存在时为 `0` | `expected<uintmax_t, error_code>` | 不存在 → 返回错误（`ENOENT` / `ERROR_FILE_NOT_FOUND`），而非 `0` |

`createDirectories` 保留了 `std::filesystem::create_directories` 的行为，目录已存在时静默成功。

---

## 注意事项

- `readDirectory` 和 `walkDirectory` 返回 `NoExcept<T>` 迭代器。反复调用 `.next()` 即可；它返回 `expected<optional<DirectoryEntry>>`，`nullopt` 的 optional 表示目录遍历结束。
- 在 Windows 上，路径字符串以 UTF-8 编码，在传递给 OS 前会内部转换为宽字符。
- 所有谓词（`isDirectory`、`isRegularFile`、`isSymlink` 等）也提供自由函数形式，封装了对应的 `std::filesystem` 函数。
