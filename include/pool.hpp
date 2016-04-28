#ifndef U_CONCURRENT_POOL
#define U_CONCURRENT_POOL

#include <thread>
#include <atomic>
#include <functional>
#include <future>

#include "queue.hpp"

namespace concurrent {

template <typename R>
class Task {
public:
    typedef std::function<R ()> Callback;
    typedef std::function<void (R)> Term;
    typedef std::shared_ptr<Task<R>> Ptr;

    Task(const Callback& c, const Term& t) : _c(c), _t(t) {}
    Task(const Callback& c) : _c(c) { _t = [] {}; }

    void Exec();

private:
    const Callback _c;

    Term _t;

    Task(Task const&) = delete;
    Task& operator=(Task const&) = delete;
};

template<>
void Task<void>::Exec() { _c(); }

template<typename R>
void Task<R>::Exec() { _t(std::move(_c())); }


template <typename R = void>
class Pool {
public:
    explicit Pool(size_t s = std::thread::hardware_concurrency()) {
        auto func = [this] {
            while (IsRunning()) {
                auto h = _msgQ.Pop(1000);
                if (h != nullptr) {
                    h->Exec();
                }
            }
        };

        for (int i = 0; i < s; i++) {
            _threads.emplace_back(func);
        }

        _guard.store(true);
    }

    virtual ~Pool() {
        Close();
    }

    bool IsRunning() const { return _guard.load(); }
    size_t Size() const { return _threads.size(); }

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

    template <typename ...Args>
    void Push(const std::function<void (Args...)>& c, Args... args) {
        if (IsRunning() == false) {
            return; //Throw
        }

        auto func = std::bind(c, args...);
        _threads.emplace_back(func);
    }

    template <typename ...Args>
    void Send(const std::function<void (Args...)>& c, Args... args) {
        auto fun = std::bind(c, args...);
        Task<void>::Ptr ptr(new Task<void>(fun));

        _msgQ.Push(ptr);
    }

    template <typename R, typename ...Args>
    void Send(const std::function<R (Args...)>& c, const std::function<void (R&&)>& t, Args... args) {
        auto fun = std::bind(c, args...);
        Task<R>::Ptr ptr(new Task<R>(fun, t));

        _msgQ.Push(ptr);
    }

private:
    SyncQueue<std::shared_ptr<Task<R>>> _msgQ;

    std::atomic_bool _guard;
    std::vector<std::thread> _threads;

    Pool(Pool const&) = delete;
    Pool& operator=(Pool const&) = delete;
};

/*
template <typename M, typename R>
using SimplePool = Pool<std::function<R (M)>, R>;

template <typename ...F>
using VariablePool = Pool<std::function<void (F...)>, void>;

using DefaultPool = Pool<std::function<void ()>, void>;
*/

}

#endif
