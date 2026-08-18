# encoding — Base64 与十六进制

头文件：
- `#include <zero/encoding/base64.h>`
- `#include <zero/encoding/hex.h>`

命名空间：`zero::encoding::base64`、`zero::encoding::hex`

---

## Base64

```cpp
#include <zero/encoding/base64.h>

// 编码
std::vector<std::byte> data = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
std::string encoded = zero::encoding::base64::encode(data);
// "AQID"

// 解码
auto result = zero::encoding::base64::decode(encoded);
if (result) {
    // *result 是 std::vector<std::byte>
} else {
    // result.error() 是 base64::DecodeError
}
```

### `DecodeError`

| 值 | 含义 |
|---|---|
| `InvalidLength` | 输入长度不是 4 的倍数 |

---

## 十六进制

```cpp
#include <zero/encoding/hex.h>

// 编码
std::vector<std::byte> data = {std::byte{0xDE}, std::byte{0xAD}};
std::string encoded = zero::encoding::hex::encode(data);
// "dead"

// 解码
auto result = zero::encoding::hex::decode("deadbeef");
if (result) {
    // *result 是 std::vector<std::byte>
}
```

### `DecodeError`

| 值 | 含义 |
|---|---|
| `InvalidLength` | 字符数为奇数 |
| `InvalidHexCharacter` | 遇到非十六进制字符 |

---

## 注意事项

- `encode()` 始终成功，返回 `std::string`。
- `decode()` 返回 `std::expected<std::vector<std::byte>, DecodeError>`。
- 十六进制输出使用小写字母（`a`–`f`）。
- 编码函数接受 `std::span<const std::byte>`，解码函数接受 `std::string_view`。
