# encoding — Base64 & Hex

Headers:
- `#include <zero/encoding/base64.h>`
- `#include <zero/encoding/hex.h>`

Namespaces: `zero::encoding::base64`, `zero::encoding::hex`

---

## Base64

```cpp
#include <zero/encoding/base64.h>

// Encode
std::vector<std::byte> data = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
std::string encoded = zero::encoding::base64::encode(data);
// "AQID"

// Decode
auto result = zero::encoding::base64::decode(encoded);
if (result) {
    // *result is std::vector<std::byte>
} else {
    // result.error() is a base64::DecodeError
}
```

### `DecodeError`

| Value | Meaning |
|---|---|
| `InvalidLength` | Input length is not a multiple of 4 |

---

## Hex

```cpp
#include <zero/encoding/hex.h>

// Encode
std::vector<std::byte> data = {std::byte{0xDE}, std::byte{0xAD}};
std::string encoded = zero::encoding::hex::encode(data);
// "dead"

// Decode
auto result = zero::encoding::hex::decode("deadbeef");
if (result) {
    // *result is std::vector<std::byte>
}
```

### `DecodeError`

| Value | Meaning |
|---|---|
| `InvalidLength` | Odd number of characters |
| `InvalidHexCharacter` | Non-hex character encountered |

---

## Notes

- `encode()` always succeeds and returns a `std::string`.
- `decode()` returns `std::expected<std::vector<std::byte>, DecodeError>`.
- Hex output uses lowercase letters (`a`–`f`).
- Both functions operate on `std::span<const std::byte>` for encode and `std::string_view` for decode.
