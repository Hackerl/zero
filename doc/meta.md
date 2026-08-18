# meta — Type Traits & Concepts

Headers:
- `#include <zero/meta/type_traits.h>`
- `#include <zero/meta/concepts.h>`

Namespace: `zero::meta`

---

## Type Traits (`type_traits.h`)

### `FunctionTraits<F>`

Introspects callable types: function pointers, member function pointers, member object pointers, lambdas, and `std::function`.

```cpp
auto f = [](int a, double b) -> std::string { return ""; };

using Traits = zero::meta::FunctionTraits<decltype(f)>;
using Ret  = Traits::ReturnType;                  // std::string
using Arg0 = Traits::template Argument<0>::Type;  // int
using Arg1 = Traits::template Argument<1>::Type;  // double
constexpr int N = Traits::Arity;                  // 2
```

Convenience aliases:
```cpp
using R = zero::meta::FunctionResult<decltype(f)>;         // std::string
using Args = zero::meta::FunctionArguments<decltype(f)>;   // std::tuple<int, double>
```

Works with `const`, `volatile`, `&`, `&&` qualified member functions.

### `IsSpecialization<T, Template>`

Checks whether `T` is a specialization of a given class template:

```cpp
static_assert(zero::meta::IsSpecialization<std::vector<int>, std::vector>);
static_assert(!zero::meta::IsSpecialization<int, std::vector>);
```

### `IsAllSame<T, Ts...>`

True when all types in the pack are the same as `T`:

```cpp
static_assert(zero::meta::IsAllSame<int, int, int>);
static_assert(!zero::meta::IsAllSame<int, int, double>);
```

### `Element<I, Ts...>` / `FirstElement<Ts...>`

Index into a type pack:

```cpp
using T = zero::meta::Element<1, int, double, float>::Type; // double
using F = zero::meta::FirstElement<int, double>::Type;      // int
```

---

## Concepts (`concepts.h`)

### `Implements<T, I>`

`T` directly inherits from `I`, or `T` is `std::shared_ptr<U>` where `U` inherits from `I`.

```cpp
template<zero::meta::Implements<IReader> T>
void readFrom(T &&reader);
```

### `OwnerOf<T, U>` / `ExclusiveOwnerOf<T, U>`

- `OwnerOf` — `T` is `U` directly or `std::shared_ptr<U>`.
- `ExclusiveOwnerOf` — `T` is `U` directly or `std::unique_ptr<U>`.

### `Specialization<T, Template>`

Concept wrapper for `IsSpecialization`:

```cpp
template<zero::meta::Specialization<std::vector> T>
void process(T &vec);
```

### `Pointer<T>`

`T` is a raw pointer type (`T*`).

### `Mutable<T>`

`T` is a non-const lvalue reference. Used in `Command` builder methods to support deducing-this:

```cpp
template<zero::meta::Mutable Self>
Self &&arg(this Self &&self, std::string a);
```

---

## Notes

- `FunctionTraits` handles cv-qualified member functions correctly as of v1.2.0 (bug fix from earlier).
- These traits are used extensively inside the library itself (e.g., `Future::fail` uses `FunctionTraits` to detect exception handler signatures).
