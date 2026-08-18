# io — I/O Abstractions

Headers:
- `#include <zero/io/io.h>` — interfaces and in-memory adapters
- `#include <zero/io/buffer.h>` — buffered reader/writer templates
- `#include <zero/io/binary.h>` — binary integer read/write

Namespace: `zero::io`

---

## Interfaces

| Interface | Key methods |
|---|---|
| `IReader` | `read(span<byte>)`, `readExactly(span<byte>)`, `readAll()` |
| `IWriter` | `write(span<byte>)`, `writeAll(span<byte>)` |
| `ISeekable` | `seek(offset, Whence)`, `rewind()`, `length()`, `position()` |
| `ICloseable` | `close()` |
| `IFileDescriptor` | `fd()` |
| `IBufReader` | extends `IReader`: `available()`, `readLine()`, `readUntil(byte)`, `peek(span)` |
| `IBufWriter` | extends `IWriter`: `pending()`, `flush()` |

`Whence` enum: `Begin`, `Current`, `End`.

All methods return `std::expected<…, std::error_code>`.

---

## In-Memory Adapters

```cpp
// Read from a string
std::string data = "hello world";
zero::io::StringReader sr{data};
std::array<std::byte, 5> buf;
sr.readExactly(buf); // fills buf with "hello"

// Read from a byte span
std::vector<std::byte> bytes = ...;
zero::io::BytesReader br{bytes};

// Write into a string
zero::io::StringWriter sw;
sw.write(std::as_bytes(std::span{"hi"}));
// sw.data() or *sw returns the accumulated string

// Write into a vector<byte>
zero::io::BytesWriter bw;
bw.write(data_span);
// bw.data() returns accumulated bytes
```

---

## Buffered I/O

`BufReader<T>` and `BufWriter<T>` wrap any `T` that implements `IReader`/`IWriter`.

```cpp
// Default buffer size: 8192 bytes
zero::io::BufReader<MyIOResource> br{std::move(resource)};

// Line-oriented reading
while (true) {
    auto line = br.readLine();
    if (!line) break; // EOF or error
    // *line is std::string (without trailing newline)
}

// Read until a delimiter byte
auto field = br.readUntil(std::byte{','});

// Peek without consuming
std::array<std::byte, 4> header;
br.peek(header);
```

```cpp
zero::io::BufWriter<MyIOResource> bw{std::move(resource)};
bw.write(data_span);
bw.flush(); // flush internal buffer to underlying writer
```

`BufReaderError` values: `InvalidArgument` (peek size exceeds buffer capacity), `UnexpectedEOF`.

---

## Binary Integer I/O

`zero::io::binary` provides endian-aware multi-byte reads and writes:

```cpp
#include <zero/io/binary.h>

// Read little-endian uint32
auto val = zero::io::binary::readLE<std::uint32_t>(reader);
if (val) { /* *val is uint32_t */ }

// Read big-endian int16
auto be = zero::io::binary::readBE<std::int16_t>(reader);

// Write
zero::io::binary::writeLE<std::uint32_t>(writer, 0xDEADBEEF);
zero::io::binary::writeBE<std::uint16_t>(writer, 0x0102);
```

Supported types: any standard arithmetic type (`uint8_t` through `uint64_t`, `float`, `double`, etc.).

---

## Copy

```cpp
// Copy all bytes from reader to writer; returns total bytes copied
std::expected<std::size_t, std::error_code> result = zero::io::copy(reader, writer);
```

---

## Notes

- `readExactly` returns `IReader::ReadExactlyError::UnexpectedEOF` if the stream ends before the requested byte count.
- `readAll()` reads until EOF and returns `vector<byte>`.
- `BufReader` / `BufWriter` take ownership of the underlying resource via move.
