# cache — LRU 缓存

`#include <zero/cache/lru.h>`

命名空间：`zero::cache`

---

## 概述

一个容量固定的最近最少使用（LRU）缓存。当缓存已满且插入新键时，最久未使用的条目会被淘汰。非线程安全——若需从多个线程访问，请使用外部锁。

---

## 模板

```cpp
template<typename K, typename V>
class LRUCache;
```

---

## 构造

```cpp
zero::cache::LRUCache<std::string, int> cache{/*capacity=*/128};
```

---

## API

```cpp
// 插入或更新
cache.set("key", 42);

// 查找（命中时更新最近使用时间）
auto val = cache.get("key");  // optional<reference_wrapper<const int>>
if (val) {
    int v = val->get();
}

// 查询（不修改最近使用时间）
bool found = cache.contains("key");

// 大小信息
std::size_t n = cache.size();
std::size_t cap = cache.capacity();
bool empty = cache.empty();
```

---

## 示例

```cpp
zero::cache::LRUCache<int, std::string> lru{3};
lru.set(1, "one");
lru.set(2, "two");
lru.set(3, "three");

lru.get(1); // 访问键 1——使键 2 成为 LRU

lru.set(4, "four"); // 淘汰键 2（LRU）
assert(!lru.contains(2));
assert(lru.contains(1));
assert(lru.contains(4));
```

---

## 注意事项

- `get()` 未命中时返回 `std::nullopt`。
- 返回的 `reference_wrapper<const V>` 在任何可能导致淘汰或替换该条目的 `set()` 调用后即失效，请勿在修改操作后继续持有该引用。
- 容量至少为 1。
