#ifndef U_CONCURRENT_QUEUE
#define U_CONCURRENT_QUEUE

#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>
#include <condition_variable>

namespace concurrent {

namespace ex {

class ClosedQueueException : public std::runtime_error {
public:
	ClosedQueueException(std::string s) : std::runtime_error(s) {}
};

}

template <typename T>
class SyncQueue {
public:
	typedef T KeyType; //TODO: fix
	typedef T ValueType;
	typedef std::shared_ptr<SyncQueue> Ptr;

    SyncQueue(size_t t = 1 << 16) : _maxSize(t), _closed(false) { }
	~SyncQueue() { }

    T Pop();
    T Pop(uint64_t ms);

    void Push(const T&);
    bool Push(const T&, uint64_t ms);

    void Push(T&&);
    bool Push(T&&, uint64_t ms);

	void WakeAndClose();

    inline bool IsEmpty() const {  std::unique_lock<std::mutex> lock(_mutex); return _queue.size() == 0; }
    inline bool IsFull() const { std::unique_lock<std::mutex> lock(_mutex); return _queue.size() == _maxSize; }
    inline size_t Size() const { std::unique_lock<std::mutex> lock(_mutex); return _queue.size(); }

    inline void Close() { 
		std::unique_lock<std::mutex> lock(_mutex); 
		_closed = true; 
		_empty.notify_all(); 
		_full.notify_all();
	}

	void Wait() {
		std::unique_lock<std::mutex> lock(_mutex);
		while (!_closed && _queue.size()) {
			_full.wait(lock);
		}
	}

	inline bool IsClosed() const { std::unique_lock<std::mutex> lock(_mutex); return _closed; }
	inline bool IsOpen() const { std::unique_lock<std::mutex> lock(_mutex); return !_closed; }
	inline bool CanReceive() const {
		std::unique_lock<std::mutex> lock(_mutex);
		return !_closed || _queue.size();
	}

protected:
    T& First() { return _queue.front(); }
    const T& First() const { return _queue.front(); }

    T& Last() { return _queue.back(); }
    const T& Last() const { return _queue.back(); }

private:
    std::queue<T> _queue;
    const size_t _maxSize;

    bool _closed;

    mutable std::mutex _mutex;
    std::condition_variable _empty;
    std::condition_variable _full;

    SyncQueue(SyncQueue const&) = delete;
    SyncQueue& operator=(SyncQueue const&) = delete;
};

template <typename T>
T SyncQueue<T>::Pop() {
    T t;

    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (_queue.size() == 0) {
            if (_closed) { 
                _full.notify_all();
				throw ex::ClosedQueueException("Pop: closed queue"); 
			}

            _empty.wait(lock);
        }

        t = _queue.front();
        _queue.pop();
    }

    _full.notify_all();
    return t;
}

template <typename T>
T SyncQueue<T>::Pop(uint64_t ms) {
    T t;

    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_queue.size() == 0) {
            if (_empty.wait_for(lock, std::chrono::milliseconds(ms)) == std::cv_status::timeout) {
				if (_closed) {
					throw ex::ClosedQueueException("Pop: closed queue");
				}
                return T();
            }

            if (_queue.size() == 0) { return T(); }
        }

        t = _queue.front();
        _queue.pop();
    }

    _full.notify_all();
    return t;
}


template <typename T>
void SyncQueue<T>::WakeAndClose() {
	std::unique_lock<std::mutex> lock(_mutex);
	if (_closed) { return; }

	_empty.notify_all();
	_queue.push(T());
	_closed = true; 
}

template <typename T>
void SyncQueue<T>::Push(const T& p) {
    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (_queue.size() == _maxSize) {
            _full.wait(lock);
        }

        _queue.push(p);
    }
    _empty.notify_all();
}

template <typename T>
bool SyncQueue<T>::Push(const T& p, uint64_t ms) {
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_queue.size() == _maxSize) {
            if (_full.wait_for(lock, std::chrono::milliseconds(ms)) == std::cv_status::timeout) {
                return false;
            }
        }

        _queue.push(p);
    }
    _empty.notify_all();
    return true;
}

template <typename T>
void SyncQueue<T>::Push(T&& p) {
    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (_queue.size() == _maxSize) {
            _full.wait(lock);
        }

        _queue.push(std::move(p));
    }
    _empty.notify_all();
}

template <typename T>
bool SyncQueue<T>::Push(T&& p, uint64_t ms) {
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_queue.size() == _maxSize) {
            if (_full.wait_for(lock, std::chrono::milliseconds(ms)) == std::cv_status::timeout) {
                return false;
            }

            if (_queue.size() == _maxSize) {
                return false;
            }
        }

        _queue.push(std::move(p));
    }
    _empty.notify_all();
    return true;
}
    
}

#endif
