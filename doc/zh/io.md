# io — I/O 抽象

头文件：
- `#include <zero/io/io.h>` — 接口与内存适配器
- `#include <zero/io/buffer.h>` — 带缓冲的读写器模板
- `#include <zero/io/binary.h>` — 二进制整数读写

命名空间：`zero::io`

---

## 接口

| 接口 | 主要方法 |
|---|---|
| `IReader` | `read(span<byte>)`、`readExactly(span<byte>)`、`readAll()` |
| `IWriter` | `write(span<byte>)`、`writeAll(span<byte>)` |
| `ISeekable` | `seek(offset, Whence)`、`rewind()`、`length()`、`position()` |
| `ICloseable` | `close()` |
| `IFileDescriptor` | `fd()` |
| `IBufReader` | 继承 `IReader`：`available()`、`readLine()`、`readUntil(byte)`、`peek(span)` |
| `IBufWriter` | 继承 `IWriter`：`pending()`、`flush()` |

`Whence` 枚举：`Begin`、`Current`、`End`。

所有方法均返回 `std::expected<…, std::error_code>`。

---

## 内存适配器

```cpp
// 从字符串读取
std::string data = "hello world";
zero::io::StringReader sr{data};
std::array<std::byte, 5> buf;
sr.readExactly(buf); // 将 "hello" 填入 buf

// 从字节 span 读取
std::vector<std::byte> bytes = ...;
zero::io::BytesReader br{bytes};

// 写入字符串
zero::io::StringWriter sw;
sw.write(std::as_bytes(std::span{"hi"}));
// sw.data() 或 *sw 返回累积的字符串

// 写入字节向量
zero::io::BytesWriter bw;
bw.write(data_span);
// bw.data() 返回累积的字节
```

---

## 带缓冲的 I/O

`BufReader<T>` 和 `BufWriter<T>` 封装任何实现了 `IReader`/`IWriter` 的类型 `T`。

```cpp
// 默认缓冲区大小：8192 字节
zero::io::BufReader<MyIOResource> br{std::move(resource)};

// 按行读取
while (true) {
    auto line = br.readLine();
    if (!line) break; // EOF 或错误
    // *line 是 std::string（不含末尾换行符）
}

// 读取直到分隔符字节
auto field = br.readUntil(std::byte{','});

// 预读而不消费
std::array<std::byte, 4> header;
br.peek(header);
```

```cpp
zero::io::BufWriter<MyIOResource> bw{std::move(resource)};
bw.write(data_span);
bw.flush(); // 将内部缓冲区刷写到底层写入器
```

`BufReaderError` 值：`InvalidArgument`（`peek` 请求大小超过缓冲区容量）、`UnexpectedEOF`。

---

## 二进制整数 I/O

`zero::io::binary` 提供端序感知的多字节读写：

```cpp
#include <zero/io/binary.h>

// 读取小端 uint32
auto val = zero::io::binary::readLE<std::uint32_t>(reader);
if (val) { /* *val 是 uint32_t */ }

// 读取大端 int16
auto be = zero::io::binary::readBE<std::int16_t>(reader);

// 写入
zero::io::binary::writeLE<std::uint32_t>(writer, 0xDEADBEEF);
zero::io::binary::writeBE<std::uint16_t>(writer, 0x0102);
```

支持所有标准算术类型（`uint8_t` 至 `uint64_t`、`float`、`double` 等）。

---

## 拷贝

```cpp
// 将 reader 中的所有字节拷贝到 writer，返回拷贝的总字节数
std::expected<std::size_t, std::error_code> result = zero::io::copy(reader, writer);
```

---

## 注意事项

- `readExactly` 若在读取到指定字节数前流就结束，返回 `IReader::ReadExactlyError::UnexpectedEOF`。
- `readAll()` 读取直到 EOF，返回 `vector<byte>`。
- `BufReader` / `BufWriter` 通过移动语义获取底层资源的所有权。
