#ifndef U_CONCURRENT_POOL
#define U_CONCURRENT_POOL

#include <thread>
#include <atomic>
#include <functional>
#include <future>

#include <iostream>

#include "queue.hpp"
#include "kv.hpp"

namespace concurrent {

namespace {

template <typename R>
class _Task {
public:
    typedef std::shared_ptr<_Task<R>> Ptr;

    _Task(const std::function<R()>& c) : _c(c) { }

    virtual void Exec();

protected:
    const std::function<R()> _c;

    _Task(_Task const&) = delete;
    _Task& operator=(_Task const&) = delete;
};

template<typename R>
void _Task<R>::Exec() { _c(); }

template <typename T>
struct _func_traits {
	typedef std::function<void(T)> FuncType;
};

template <>
struct _func_traits<void> {
	typedef std::function<void()> FuncType;
};
 

template <typename R>
class Task : public _Task<R> {
public:
    typedef std::shared_ptr<Task<R>> Ptr;

    Task(const std::function<R()>& c, const typename _func_traits<R>::FuncType& t) : _Task<R>(c), _t(t) {}
    Task(const std::function<R()>& c) : _Task<R>(c) { _t = [] (R) {}; }

    virtual void Exec();

private:
	typename _func_traits<R>::FuncType _t;
};

template<>
Task<void>::Task(const std::function<void()>& c) : _Task<void>(c) { _t = []() {}; }

template<>
void Task<void>::Exec() { this->_c(); this->_t(); }

template<typename R>
void Task<R>::Exec() { this->_t(std::move(this->_c())); }

class _Pool {
public:
	typedef std::shared_ptr<_Pool> Ptr;
	virtual ~_Pool() { }

	virtual void Close() = 0;
};

}

class WaitGroup {
public:
    typedef std::shared_ptr<WaitGroup> Ptr;

    WaitGroup(size_t s) : _s(s) {
        for (int i = 0; i < s; i++) {
            _group.Push(i);
        }
    }

    size_t Size() const { return _s; }

    void Finish() {
        _group.Pop();
        if (!_group.Size()) {
            _empty.notify_all();
        }
    }

    void Wait() {
        std::unique_lock<std::mutex> lock(_mutex);
        _empty.wait(lock);
    }

private:
    const size_t _s;
    SyncQueue<int> _group;

    mutable std::mutex _mutex;
    std::condition_variable _empty;


    WaitGroup(WaitGroup const&) = delete;
    WaitGroup& operator=(WaitGroup const&) = delete;

};

template <typename R = void, typename ...Args>
class Pool : public _Pool {
public:
    typedef std::shared_ptr<Pool<R, Args...>> Ptr;

    explicit Pool(size_t s = std::thread::hardware_concurrency()) {init(s); }
    explicit Pool(const std::function<R (Args...)>& c, size_t s = std::thread::hardware_concurrency()) : _c(c) { init(s);}
    explicit Pool(const std::function<R (Args...)>& c,
                  const typename _func_traits<R>::FuncType& t,
                  size_t s = std::thread::hardware_concurrency()) : _c(c), _t(t)
    { init(s); }

    virtual ~Pool() { Close(); }

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

    template <typename ..._Args>
    void Send(const std::function<R (_Args...)>& c, const typename _func_traits<R>::FuncType& t, _Args... args) {
        auto fun = std::bind(c, args...);
        typename Task<R>::Ptr ptr(new Task<R>(fun, t));

        _msgQ.Push(ptr);
    }

    void Send(const std::function<void ()>& c, size_t num = 1) {
        for (int i = 0; i < num; i++) {
            _Task<void>::Ptr ptr(new _Task<void>(c));
            _msgQ.Push(ptr);
        }
    }

    void Send(const std::function<R ()>& c, const typename _func_traits<R>::FuncType& t) {
        typename Task<R>::Ptr ptr(new Task<R>(c, t));
        _msgQ.Push(ptr);
    }

    template <typename ..._Args>
    void Call(_Args... args) {
        auto fun = std::bind(_c, args...);
        typename Task<R>::Ptr ptr(new Task<R>(fun));

        _msgQ.Push(ptr);
    }

    virtual void Close() {
        _guard.store(false);

        if (_threads.empty()) {
            return;
        }

        for (auto i = 0; i < _threads.size(); i++) {
            _msgQ.Push(nullptr);
        }

        for (auto& t : _threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        _threads.clear();
    }

private:
    void init(size_t s) noexcept {
        _guard.store(true);

        auto func = [this] {
            while (IsRunning()) {
				try {
					auto h = _msgQ.Pop();
					if (h != nullptr) {
                        h->Exec();
					}
				} catch (const std::exception& e) {
					_ee(e);
				}
            }
        };

        for (int i = 0; i < s; i++) {
            _threads.emplace_back(func);
        }

        _ee = [](const std::exception& e) { std::cerr << "Error: " << e.what() << std::endl; };
    }

    SyncQueue<std::shared_ptr<_Task<R>>> _msgQ;
    std::vector<std::thread> _threads;

    std::atomic_bool _guard;
    mutable std::mutex _mutex;


    Pool(Pool const&) = delete;
    Pool& operator=(Pool const&) = delete;

    const std::function<R (Args...)> _c;
    const typename _func_traits<R>::FuncType _t;
	std::function<void(const std::exception&)> _ee;
};

template <typename R = void>
inline Pool<R>& SystemTaskPool() {
    static Pool<R> pool;
    return pool;
}

namespace /*anonimous*/ {

template <typename I, typename O, typename ..._Args>
class _StreamItem {
public:
    typedef std::shared_ptr<_StreamItem<I, O, _Args...>> Ptr;

    _StreamItem(_Args... a) : _pool(new Pool<void, _Args...>(a...)), _in(new O()), _out(_in) {}
    _StreamItem(typename I::Ptr i, _Args... a) : _pool(new Pool<void, _Args...>(a...)), _in(i), _out(new O()) {}
    _StreamItem(typename I::Ptr i, typename Pool<void, _Args...>::Ptr p) : _pool(p), _in(i), _out(new O()) {}

	virtual ~_StreamItem() {
		if (_pool->IsRunning()) {
			_pool->Close();
		}
	}

    typename I::Ptr Input() { return _in; }
    typename O::Ptr Output() { return _out; }

	template <typename _I, typename _K, typename _V>
	using Mapper = _StreamItem<SyncQueue<_I>, SyncMap<_K, _V>>;

    template <typename _I, typename _K, typename _V>
    typename Mapper<_I, _K, _V>::Ptr Map(const std::function<std::pair<_K, _V> (_I)>& fn) {
        typename Mapper<_I, _K, _V>::Ptr item(new Mapper<_I, _K, _V>(_out, _pool));

        _pool->Send([this, item, fn] {
            while (item->Input()->CanReceive()) {
                auto ret = fn(item->Input()->Pop());
                item->Output()->Insert(ret.first, ret.second);
            }

            item->Output()->Close();
        });

        return item;
    }

	template <typename _I>
	using Bouncer = _StreamItem<SyncQueue<_I>, SyncQueue<_I>>;

	template <typename _I>
	typename Bouncer<_I>::Ptr Filter(const std::function<bool (_I)>& fn) { 
        typename Bouncer<_I>::Ptr item(new Bouncer<_I>(_out, _pool));
		
		_pool->Send([item, fn] {
            while (item->Input()->CanReceive()) {
                auto val = item->Input()->Pop();
                auto ret = fn(val);
                if (ret) {
                    item->Output()->Push(val);
                }
            }
			item->Output()->Close();
        }, 2);

		return item;
	}

	template <typename _K, typename _V, typename _O>
	using Collector = _StreamItem<SyncMap<_K, _V>, SyncQueue<_O>>;

	template <typename _K, typename _V, typename _O>
	typename Collector<_K, _V, _O>::Ptr Collect(const std::function<_O(_K, _V)>& fn) {
        typename Collector<_K, _V, _O>::Ptr item(new Collector<_K, _V, _O>(_out, _pool));

        _pool->Send([item, fn] {
            item->Input()->Wait();
			item->Input()->ForEach([item, fn] (const std::pair<_K, _V>& pair) {
				auto o = fn(pair.first, pair.second);
				item->Output()->Push(o);
			});
		
		}, 2);

		return item;
	}

private:
    typename Pool<void, _Args...>::Ptr _pool;

    typename I::Ptr _in;
    typename O::Ptr _out;
};

}

template <typename _I>
using Streamer = _StreamItem<SyncQueue<_I>, SyncQueue<_I>>;

template <typename _I, typename _K, typename _V>
using Mapper = _StreamItem<SyncQueue<_I>, SyncMap<_K, _V>>;

template <typename _I>
using Bouncer = _StreamItem<SyncQueue<_I>, SyncQueue<_I>>;

template <typename _K, typename _V, typename _O>
using Collector = _StreamItem<SyncMap<_K, _V>, SyncQueue<_O>>;

template <typename _I, typename _O>
using Reducer = _StreamItem<SyncQueue<_I>, _O>;

}

#endif
