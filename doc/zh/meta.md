# meta — 类型特征与概念

头文件：
- `#include <zero/meta/type_traits.h>`
- `#include <zero/meta/concepts.h>`

命名空间：`zero::meta`

---

## 类型特征 (`type_traits.h`)

### `FunctionTraits<F>`

对可调用类型进行自省：函数指针、成员函数指针、成员对象指针、lambda 和 `std::function`。

```cpp
auto f = [](int a, double b) -> std::string { return ""; };

using Traits = zero::meta::FunctionTraits<decltype(f)>;
using Ret  = Traits::ReturnType;                  // std::string
using Arg0 = Traits::template Argument<0>::Type;  // int
using Arg1 = Traits::template Argument<1>::Type;  // double
constexpr int N = Traits::Arity;                  // 2
```

便捷别名：
```cpp
using R    = zero::meta::FunctionResult<decltype(f)>;       // std::string
using Args = zero::meta::FunctionArguments<decltype(f)>;    // std::tuple<int, double>
```

支持带 `const`、`volatile`、`&`、`&&` 限定符的成员函数。

### `IsSpecialization<T, Template>`

检查 `T` 是否为给定类模板的特化：

```cpp
static_assert(zero::meta::IsSpecialization<std::vector<int>, std::vector>);
static_assert(!zero::meta::IsSpecialization<int, std::vector>);
```

### `IsAllSame<T, Ts...>`

当包中所有类型都与 `T` 相同时为 true：

```cpp
static_assert(zero::meta::IsAllSame<int, int, int>);
static_assert(!zero::meta::IsAllSame<int, int, double>);
```

### `Element<I, Ts...>` / `FirstElement<Ts...>`

按索引访问类型包：

```cpp
using T = zero::meta::Element<1, int, double, float>::Type; // double
using F = zero::meta::FirstElement<int, double>::Type;      // int
```

---

## 概念 (`concepts.h`)

### `Implements<T, I>`

`T` 直接继承自 `I`，或 `T` 是 `std::shared_ptr<U>`（其中 `U` 继承自 `I`）。

```cpp
template<zero::meta::Implements<IReader> T>
void readFrom(T &&reader);
```

### `OwnerOf<T, U>` / `ExclusiveOwnerOf<T, U>`

- `OwnerOf` — `T` 是 `U` 本身或 `std::shared_ptr<U>`。
- `ExclusiveOwnerOf` — `T` 是 `U` 本身或 `std::unique_ptr<U>`。

### `Specialization<T, Template>`

`IsSpecialization` 的概念封装：

```cpp
template<zero::meta::Specialization<std::vector> T>
void process(T &vec);
```

### `Pointer<T>`

`T` 是原始指针类型（`T*`）。

### `Mutable<T>`

`T` 是非 const 左值引用。用于 `Command` 构建器方法中的推导 this（deducing-this）：

```cpp
template<zero::meta::Mutable Self>
Self &&arg(this Self &&self, std::string a);
```

---

## 注意事项

- `FunctionTraits` 从 v1.2.0 起正确处理带 cv 限定符的成员函数（修复了之前的 bug）。
- 这些特征在库内部被广泛使用（例如，`Future::fail` 使用 `FunctionTraits` 检测异常处理器的签名）。
