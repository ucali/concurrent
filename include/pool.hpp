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
	typedef typename _func_traits<R>::FuncType FuncType;

    Task(const std::function<R()>& c, const FuncType& t) : _Task<R>(c), _t(t) {}
    Task(const std::function<R()>& c) : _Task<R>(c) { _t = [] (R) {}; }

    virtual void Exec();

private:
	FuncType _t;
};

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
                  const std::function<void (R)>& t,
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
    void Send(const std::function<R (_Args...)>& c, const std::function<void (R)>& t, _Args... args) {
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

    void Send(const std::function<R ()>& c, const std::function<void (R)>& t) {
        typename Task<R>::Ptr ptr(new Task<R>(c, t));
        _msgQ.Push(ptr);
    }

    template <typename ..._Args>
    void Call(_Args... args) {
        auto fun = std::bind(_c, args...);
        typename Task<R>::Ptr ptr(new Task<R>(fun));

        _msgQ.Push(ptr);
    }

    /*PROCESS

    template <typename _I, typename _K, typename _V>
    using Mapper = Pool<void, typename SyncQueue<_I>::Ptr, typename SyncMap<_K, _V>::Ptr>;

    template <typename _I, typename _K, typename _V>
    typename Mapper<_I, _K, _V>::Ptr
    Map(typename SyncQueue<_I>::Ptr in, typename SyncMap<_K, _V>::Ptr out, const std::function<void (typename SyncQueue<_I>::Ptr, typename SyncMap<_K, _V>::Ptr)>& op) {
        Mapper<_I, _K, _V>::Ptr pool(new Mapper<_I, _K, _V>(op));
        pool->Call<typename SyncQueue<_I>::Ptr, typename SyncMap<_K, _V>::Ptr>(in, out);
        return pool;
    }

    template <typename _I, typename _O>
    using ListCollector = Pool<void, typename SyncQueue<_I>::Ptr, typename SyncQueue<_O>::Ptr>;

    template <typename _I, typename _O>
    typename ListCollector<_I, _O>::Ptr
    Collect(typename SyncQueue<_I>::Ptr in, typename SyncQueue<_O>::Ptr out, const std::function<void (typename SyncQueue<_I>::Ptr, typename SyncQueue<_O>::Ptr)>& op) {
        ListCollector<_I, _O>::Ptr pool(new ListCollector<_I, _O>(op));
        pool->Call<typename SyncQueue<_I>::Ptr, typename SyncQueue<_O>::Ptr>(in, out);
        return pool;
    }

    template <typename _K, typename _V, typename _O>
    using MapCollector = Pool<void, typename SyncMap<_K, _V>::Ptr, typename SyncQueue<_O>::Ptr>;

    template <typename _K, typename _V, typename _O>
    typename MapCollector<_K, _V, _O>::Ptr Collect(typename SyncMap<_K, _V>::Ptr in, typename SyncQueue<_O>::Ptr out, const std::function<void (typename SyncMap<_K, _V>::Ptr, typename SyncQueue<_O>::Ptr)>& op) {
        MapCollector<_K, _V, _O>::Ptr pool(new MapCollector<_K, _V, _O>(op));
        pool->Call<typename SyncMap<_K, _V>::Ptr, typename SyncQueue<_O>::Ptr>(in, out);
        return pool;
    }

    template <typename _I>
    using ListFilter = Pool<void, typename SyncQueue<_I>::Ptr, typename SyncQueue<_I>::Ptr>;

    template <typename _I>
    typename ListFilter<_I>::Ptr Filter(typename SyncQueue<_I>::Ptr in, typename SyncQueue<_I>::Ptr out, const std::function<void (typename SyncQueue<_I>::Ptr, typename SyncQueue<_I>::Ptr)>& op) {
        ListFilter<_I>::Ptr pool(new ListFilter<_I>(op));
        pool->Call<typename SyncQueue<_I>::Ptr, typename SyncQueue<_I>::Ptr>(in, out);
        return pool;
    }*/

    virtual void Close() {
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
					auto h = _msgQ.Pop();
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

    SyncQueue<std::shared_ptr<_Task<R>>> _msgQ;

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
inline Pool<R>& SystemTaskPool() {
    static Pool<R> pool;
    return pool;
}

namespace /*anonimous*/ {

template <typename I, typename O, typename ..._Args>
class _StreamItem : public Pool<void, _Args...> {
public:
    typedef std::shared_ptr<_StreamItem<I, O, _Args...>> Ptr;

    _StreamItem(_Args... a) : Pool<void, _Args...>(a...), _in(new O()), _out(_in) {}
    _StreamItem(typename I::Ptr i, _Args... a) : Pool<void, _Args...>(a...), _in(i), _out(new O()) {}

	virtual ~_StreamItem() {
		Close();
		if (_child) { 
			_child->Close(); 
		} 
	}

    typename I::Ptr Input() { return _in; }
    typename O::Ptr Output() { return _out; }

    template <typename _I, typename _K, typename _V>
    typename _StreamItem<typename SyncQueue<_I>, typename SyncMap<_K, _V>>::Ptr Map(const std::function<std::pair<_K, _V> (_I)>& fn) {
		_StreamItem<typename SyncQueue<_I>, typename SyncMap<_K, _V>>::Ptr item(new _StreamItem<typename SyncQueue<_I>, typename SyncMap<_K, _V>>(_out));

        item->Send([this, item, fn] {
            while (item->Input()->CanReceive()) {
                auto ret = fn(item->Input()->Pop());
                item->Output()->Insert(ret.first, ret.second);
            }

        }, 2);

		_child = item;
        return item;
    }

	template <typename _I>
	typename _StreamItem<typename SyncQueue<_I>, typename SyncQueue<_I>> Filter(const std::function<bool (_I)>& fn) {
		_StreamItem<typename SyncQueue<_I>, typename SyncQueue<_I>>::Ptr item(new _StreamItem<typename SyncQueue<_I>, typename SyncQueue<_I>>(_out));
		
		item->Send([item] {
			while (item->Input()->CanReceive()) {
				while (item->Input()->CanReceive()) {
					auto pop = val;
					auto ret = fn(val);
					if (ret) {
						item->Output()->Push(val);
					}
				}
			}
		}, 2);

		_child = item;
		return item;
	}

	template <typename _K, typename _V, typename _O>
	typename _StreamItem<typename SyncMap<_K, _V>, typename SyncQueue<_O>>::Ptr Reduce(const std::function<_O(_K, _V)>& fn) {
		_StreamItem<typename SyncMap<_K, _V>, typename SyncQueue<_O>>::Ptr item(new _StreamItem<typename SyncMap<_K, _V>, typename SyncQueue<_O>>(_out));
		item->Send([item, fn] {
			item->Input()->ForEach([item, fn] (const std::pair<_K, _V>& pair) {
				auto o = fn(pair.first, pair.second);
				item->Output()->Push(o);
			});
		
		}, 2);

		_child = item;
		return item;
	}


private:
    typename I::Ptr _in;
    typename O::Ptr _out;

	_Pool::Ptr _child;
};

}

template <typename _I>
using Streamer = _StreamItem<typename SyncQueue<_I>, typename SyncQueue<_I>>;

template <typename _I, typename _K, typename _V>
using Mapper = _StreamItem<typename SyncQueue<_I>, typename SyncMap<_K, _V>>;

template <typename _I>
using Bouncer = _StreamItem<typename SyncQueue<_I>, typename SyncQueue<_I>>;

template <typename _K, typename _V, typename _O>
using Reducer = _StreamItem<typename SyncMap<_K, _V>, typename SyncQueue<_O>>;

}

#endif
