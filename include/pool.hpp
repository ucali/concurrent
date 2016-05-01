#ifndef U_CONCURRENT_POOL
#define U_CONCURRENT_POOL

#include <thread>
#include <atomic>
#include <functional>
#include <future>

#include <iostream>

#include "queue.hpp"

namespace concurrent {

namespace {

template <typename R>
class Task {
public:
    typedef std::function<R ()> Callback;
    typedef std::function<void (R)> Term;
    typedef std::shared_ptr<Task<R>> Ptr;

    Task(const Callback& c, const Term& t) : _c(c), _t(t) {}
    Task(const Callback& c) : _c(c) { _t = [] (R) {}; }

    void Exec();

private:
    const Callback _c;

    Term _t;

    Task(Task const&) = delete;
    Task& operator=(Task const&) = delete;
};

template<>
void Task<void>::Exec() { _c(); _t(); }

template<typename R>
void Task<R>::Exec() { _t(std::move(_c())); }

}

template <typename R = void, typename ...Args>
class Pool {
public:
    explicit Pool(size_t s = std::thread::hardware_concurrency()) {init(s); }
    explicit Pool(const std::function<R (Args...)>& c, size_t s = std::thread::hardware_concurrency()) : _c(c) { init(s);}
    explicit Pool(const std::function<R (Args...)>& c,
                  const std::function<void (R)>& t,
                  size_t s = std::thread::hardware_concurrency()) : _c(c), _t(t)
    { init(s); }

    ~Pool() { Close(); }

    bool IsRunning() const { return _guard.load(); }
    size_t Size() const { std::unique_lock<std::mutex> lock(_mutex); return _threads.size(); }

	void OnException(const std::function<void(const std::exception&)>& f) { _ee = f; }

    template <typename ..._Args>
    void Spawn(const std::function<void (_Args...)>& c, _Args... args) {
        if (IsRunning() == false) {
            return; //Throw
        }

        auto func = std::bind(c, args...);

        std::unique_lock<std::mutex> lock(_mutex);
        _threads.emplace_back(func);
    }

    void Spawn(const std::function<void ()>& c) {
        if (IsRunning() == false) {
            return; //Throw
        }

        std::unique_lock<std::mutex> lock(_mutex);
        _threads.emplace_back(c);
    }

    template <typename ..._Args>
    void Send(const std::function<void (_Args...)>& c, _Args... args) {
        auto fun = std::bind(c, args...);
        Task<void>::Ptr ptr(new Task<void>(fun));

        _msgQ.Push(ptr);
    }

    void Send(const std::function<void ()>& c) {
        Task<void>::Ptr ptr(new Task<void>(c));
        _msgQ.Push(ptr);
    }

    template <typename ...Args>
    void Send(const std::function<R (Args...)>& c, const std::function<void (R)>& t, Args... args) {
        auto fun = std::bind(c, args...);
        Task<R>::Ptr ptr(new Task<R>(fun, t));

        _msgQ.Push(ptr);
    }

    void Send(const std::function<R ()>& c, const std::function<void (R)>& t) {
        Task<R>::Ptr ptr(new Task<R>(c, t));
        _msgQ.Push(ptr);
    }

    template <typename ..._Args>
    void Call(_Args... args) {
        auto fun = std::bind(_c, args...);
        Task<void>::Ptr ptr(new Task<void>(fun));

        _msgQ.Push(ptr);
    }

protected:
    void Close() {
        _guard.store(false);

        if (_threads.empty()) {
            return;
        }

        for (auto i = 0; i < _threads.size(); i++) {
            _msgQ.Push(nullptr);
        }

        for (auto& t : _threads) {
            t.join();
        }

        _threads.clear();
    }

private:
    void init(size_t s) noexcept {
        _guard.store(true);

        auto func = [this] {
            while (IsRunning()) {
				try {
					auto h = _msgQ.Pop(1000);
					if (h != nullptr) {
						h->Exec();
					}
				} catch (const std::exception& e) {
					_ee(e);
				}
            }
        };

		_ee = [](const std::exception& e) { std::cerr << "Error: " << e.what() << std::endl; };

        for (int i = 0; i < s; i++) {
            _threads.emplace_back(func);
        }
    }

    SyncQueue<std::shared_ptr<Task<R>>> _msgQ;

    std::atomic_bool _guard;
    mutable std::mutex _mutex;

    std::vector<std::thread> _threads;

    Pool(Pool const&) = delete;
    Pool& operator=(Pool const&) = delete;

    const std::function<R (Args...)> _c;
    const std::function<void (R)> _t;
	std::function<void(const std::exception&)> _ee;
};

template <typename R = void>
inline Pool<R>& DefaultPool() {
    static Pool<R> pool;
    return pool;
}

/*
template <typename M, typename R>
using SimplePool = Pool<std::function<R (M)>, R>;

template <typename ...F>
using VariablePool = Pool<std::function<void (F...)>, void>;

using DefaultPool = Pool<std::function<void ()>, void>;
*/

}

#endif
