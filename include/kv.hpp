#ifndef U_CONCURRENT_KV_HPP
#define U_CONCURRENT_KV_HPP

#include <map>
#include <unordered_map>
#include <condition_variable>

#include <mutex>

namespace concurrent {

namespace {

template <typename _M, typename _K, typename _V>
class _SyncMap {
public:
	typedef std::shared_ptr<_SyncMap<_M, _K, _V>> Ptr;

	_SyncMap() {}
	virtual ~_SyncMap() { Close(); }

    void Insert(const _K& k, const _V& v) {
        std::unique_lock<std::mutex> lock(_mutex);
        _map.insert(std::pair<_K, _V>(k, v));
    }

    const _V& Get(const _K& k) const {
        std::unique_lock<std::mutex> lock(_mutex);
        return _map.at(k);
    }

    bool Remove(const _K& k) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _map.find(k);
        if (it == _map.end()) {
            return false;
        }

        _map.erase(it);
        return true;
    }

    bool Find(const _K& k) {
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

	void ForEach(const std::function<void(const std::pair<_K, _V>&)>& fn) const {
		std::unique_lock<std::mutex> lock(_mutex);
		std::for_each(_map.begin(), _map.end(), fn);
	}

    void Close() {
		{
			std::unique_lock<std::mutex> lock(_mutex);
			if (_opened) {
				_opened = false;
			}
		}
		_waiter.notify_all();
    }

    void Wait() {
        std::unique_lock<std::mutex> lock(_mutex);
		if (_opened) {
			_waiter.wait(lock);
		}
    }

private:
	bool _opened = true;
	
    mutable std::mutex _mutex;
    std::condition_variable _waiter;

    _M _map;

	_SyncMap(_SyncMap const&) = delete;
	_SyncMap& operator=(_SyncMap const&) = delete;
};

}

template <typename _K, typename _V>
using SyncMap = _SyncMap<std::map<_K, _V>, _K, _V>;

template <typename _K, typename _V>
using SyncHashMap = _SyncMap<std::map<_K, _V>, _K, _V>;

}

#endif // U_CONCURRENT_MAP_HPP
