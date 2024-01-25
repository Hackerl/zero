#ifndef ZERO_LRU_H
#define ZERO_LRU_H

#include <list>
#include <optional>
#include <unordered_map>

namespace zero::cache {
    template<typename K, typename V>
    class LRUCache {
    public:
        explicit LRUCache(const std::size_t capacity) : mCapacity(capacity) {
        }

        void set(const K &key, const V &value) {
            const auto it = mMap.find(key);

            if (it != mMap.end()) {
                if (it->second.second == mKeys.begin()) {
                    it->second.first = value;
                    return;
                }

                mKeys.erase(it->second.second);
                mKeys.push_front(key);

                it->second.first = value;
                it->second.second = mKeys.begin();

                return;
            }

            mKeys.push_front(key);
            mMap[key] = {value, mKeys.begin()};

            if (size() > mCapacity) {
                mMap.erase(mKeys.back());
                mKeys.pop_back();
            }
        }

        std::optional<V> get(const K &key) {
            const auto it = mMap.find(key);

            if (it == mMap.end())
                return std::nullopt;

            if (it->second.second == mKeys.begin())
                return it->second.first;

            mKeys.erase(it->second.second);
            mKeys.push_front(key);

            it->second.second = mKeys.begin();

            return it->second.first;
        }

        [[nodiscard]] bool contains(const K &key) const {
            return mMap.contains(key);
        }

        [[nodiscard]] std::size_t size() const {
            return mMap.size();
        }

        [[nodiscard]] std::size_t capacity() const {
            return mCapacity;
        }

        [[nodiscard]] bool empty() const {
            return mMap.empty();
        }

    private:
        std::size_t mCapacity;
        std::list<K> mKeys;
        std::unordered_map<K, std::pair<V, typename std::list<K>::iterator>> mMap;
    };
}

#endif //ZERO_LRU_H
