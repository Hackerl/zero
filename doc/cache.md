# cache — LRU Cache

`#include <zero/cache/lru.h>`

Namespace: `zero::cache`

---

## Overview

A fixed-capacity least-recently-used cache. When the cache is full and a new key is inserted, the least-recently-used entry is evicted. Not thread-safe — protect with an external lock if accessed from multiple threads.

---

## Template

```cpp
template<typename K, typename V>
class LRUCache;
```

---

## Construction

```cpp
zero::cache::LRUCache<std::string, int> cache{/*capacity=*/128};
```

---

## API

```cpp
// Insert or update
cache.set("key", 42);

// Look up (updates recency on hit)
auto val = cache.get("key");  // optional<reference_wrapper<const int>>
if (val) {
    int v = val->get();
}

// Query without modifying recency
bool found = cache.contains("key");

// Size info
std::size_t n = cache.size();
std::size_t cap = cache.capacity();
bool empty = cache.empty();
```

---

## Example

```cpp
zero::cache::LRUCache<int, std::string> lru{3};
lru.set(1, "one");
lru.set(2, "two");
lru.set(3, "three");

lru.get(1); // access key 1 — makes 2 the LRU

lru.set(4, "four"); // evicts key 2 (LRU)
assert(!lru.contains(2));
assert(lru.contains(1));
assert(lru.contains(4));
```

---

## Notes

- `get()` returns `std::nullopt` on a miss.
- The returned `reference_wrapper<const V>` is invalidated if any `set()` call evicts or replaces the referenced entry. Do not hold the reference across mutations.
- Capacity must be at least 1.
