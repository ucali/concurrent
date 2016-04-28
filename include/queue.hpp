#ifndef U_CONCURRENT_QUEUE
#define U_CONCURRENT_QUEUE

#include <mutex>
#include <queue>
#include <chrono>
#include <type_traits>

namespace concurrent {

template <typename T>
class SyncQueue {
public:
    SyncQueue(size_t t = 1 << 16) : _maxSize(t) {}

    T Pop();
    T Pop(uint64_t ms);

    void Push(const T&);
    bool Push(const T&, uint64_t ms);

    void Push(T&&);
    bool Push(T&&, uint64_t ms);

    T& Front() { return _queue.front(); }
    const T& Front() const { return _queue.front(); }

    T& Back() { return _queue.back(); }
    const T& Back() const { return _queue.back(); }

    bool IsEmpty() const { return _queue.size() == 0; }
    bool IsFull() const { return _queue.size() == _maxSize; }
    size_t Size() const { return _queue.size(); }

private:
    std::queue<T> _queue;
    const size_t _maxSize;

    mutable std::mutex _mutex;
    std::condition_variable _empty;
    std::condition_variable _full;

    SyncQueue(SyncQueue const&) = delete;
    SyncQueue& operator=(SyncQueue const&) = delete;
};

template <typename T>
T SyncQueue<T>::Pop() {
    T back;

    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (IsEmpty()) {
            _empty.wait(lock);
        }

        back = _queue.back();
        _queue.pop();
    }

    _full.notify_all();
    return back;
}

template <typename T>
T SyncQueue<T>::Pop(uint64_t ms) {
    T back;

    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (IsEmpty()) {
            if (_empty.wait_for(lock, std::chrono::milliseconds(ms)) == std::cv_status::timeout) {
                return T();
            }

            if (IsEmpty()) {
                return T();
            }
        }

        back = _queue.back();
        _queue.pop();
    }

    _full.notify_all();
    return back;
}

template <typename T>
void SyncQueue<T>::Push(const T& p) {
    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (IsFull()) {
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
        if (IsFull()) {
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
        while (IsFull()) {
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
        if (IsFull()) {
            if (_full.wait_for(lock, std::chrono::milliseconds(ms)) == std::cv_status::timeout) {
                return false;
            }

            if (IsFull()) {
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
