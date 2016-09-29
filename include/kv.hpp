#ifndef U_CONCURRENT_KV_HPP
#define U_CONCURRENT_KV_HPP

#include <map>
#include <unordered_map>
#include <condition_variable>

#include <mutex>

namespace concurrent {

template <typename _M>
class _SyncMap {
public:
	typedef std::shared_ptr<_SyncMap<_M>> Ptr;

	typedef typename _M::key_type KeyType;
	typedef typename _M::mapped_type ValueType;

	typedef typename _M::value_type PairType;
	typedef typename std::pair<KeyType, ValueType> Type;

	_SyncMap() {}
	~_SyncMap() { Close(); }

    void Insert(const KeyType& k, const ValueType& v) {
        std::unique_lock<std::mutex> lock(_mutex);
        _map.insert(std::make_pair(k, v));
    }

	void Insert(const Type& t) {
		std::unique_lock<std::mutex> lock(_mutex);
		_map.insert(t);
	}

    bool Remove(const KeyType& k) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _map.find(k);
        if (it == _map.end()) {
            return false;
        }

        _map.erase(it);
        return true;
    }

    auto Find(const KeyType& k) {
        std::unique_lock<std::mutex> lock(_mutex);
        return _map.find(k);
    }

	auto End() {
		std::unique_lock<std::mutex> lock(_mutex);
		return _map.end();
	}

	bool Contains(const KeyType& k) {
		std::unique_lock<std::mutex> lock(_mutex);
		auto it = _map.find(k);
		if (it == _map.end()) {
			return false;
		}
		return true;
	}

    void Clear() {
        std::unique_lock<std::mutex> lock(_mutex);
        _map.clear();
    }

    size_t Size () const {
        std::unique_lock<std::mutex> lock(_mutex);
        return _map.size();
    }

	void ForEach(const std::function<void(const Type&)>& fn) const {
		std::unique_lock<std::mutex> lock(_mutex);
		std::for_each(_map.begin(), _map.end(), fn);
	}

	template <typename Storage>
	void Aggregate(const std::function<void(Storage&&)>& fn) const {
		std::set<KeyType> keys;

		std::unique_lock<std::mutex> lock(_mutex);

		std::for_each(_map.begin(), _map.end(), [&keys](auto t) { keys.insert(t.first); });
		for (const KeyType& t : keys) {
			auto it = _map.equal_range(t); 
			Storage storage;
			storage.reserve(std::distance(it.first, it.second));
			std::transform(it.first, it.second, std::back_inserter(storage), [](auto element) { return element.second; });

			fn(std::move(storage));
		}
	}

    void Close() {
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_opened = false;
		}
		_waiter.notify_all();
    }

    void Wait() {
        std::unique_lock<std::mutex> lock(_mutex);
		while (_opened) {
			_waiter.wait(lock);
		}
    }

protected:
	bool _opened = true;
	
    mutable std::mutex _mutex;
    std::condition_variable _waiter;

    _M _map;

	_SyncMap(_SyncMap const&) = delete;
	_SyncMap& operator=(_SyncMap const&) = delete;
};

template <typename _K, typename _V>
using SyncMap = _SyncMap<std::map<_K, _V>>;

template <typename _K, typename _V>
using SyncHashMap = _SyncMap<std::unordered_map<_K, _V>>;

template <typename _K, typename _V>
using SyncMultiMap = _SyncMap<std::multimap<_K, _V>>;

}

#endif // U_CONCURRENT_MAP_HPP
