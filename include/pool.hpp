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

class _Stub {
public:
	typedef std::shared_ptr<_Stub> Ptr;
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
        _empty.notify_all();
    }

    void Wait() {
		while (_group.Size()) {
			std::unique_lock<std::mutex> lock(_mutex);
			_empty.wait(lock);
		}
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
class Pool {
public:
    typedef std::shared_ptr<Pool<R, Args...>> Ptr;

    explicit Pool(size_t s = std::thread::hardware_concurrency()) { init(s); }
    explicit Pool(const std::function<R (Args...)>& c, size_t s = std::thread::hardware_concurrency()) : _c(c) { init(s);}
    explicit Pool(const std::function<R (Args...)>& c,
                  const typename _func_traits<R>::FuncType& t,
                  size_t s = std::thread::hardware_concurrency()) : _c(c), _t(t)
    { init(s); }

	~Pool() {  Close(); }

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

    void Close() {
		try {
			std::unique_lock<std::mutex> lock(_mutex);

			if (_threads.empty()) {
				return;
			}

			_guard.store(false);

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
		catch (...) {
			std::cerr << "Error shutting down pool." << std::endl; 
		}
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

namespace /*_*/ {

template <typename I, typename O, typename ..._Args>
class _StreamItem  {
public:
    typedef std::shared_ptr<_StreamItem<I, O, _Args...>> Ptr;

    _StreamItem(_Args... a) : _pool(new Pool<void, _Args...>(a...)), _in(new O()), _out(_in) {}
    _StreamItem(typename I::Ptr i, _Args... a) : _pool(new Pool<void, _Args...>(a...)), _in(i), _out(new O()) {}

	_StreamItem(typename I::Ptr i, typename Pool<void, _Args...>::Ptr p) : _in(i), _out(new O()) { _pool = p; }

	~_StreamItem() {
		_in->Close();
	}

    typename I::Ptr Input() { return _in; }
    typename O::Ptr Output() { return _out; }

	template <typename _I, typename _K, typename _V>
	using Mapper = _StreamItem<SyncQueue<_I>, SyncMap<_K, _V>>;

    template <typename _I, typename _K, typename _V>
    typename Mapper<_I, _K, _V>::Ptr Map(const std::function<std::pair<_K, _V> (_I)>& fn, size_t s = 1) {
        typename Mapper<_I, _K, _V>::Ptr item(new Mapper<_I, _K, _V>(_out, _pool));

		WaitGroup::Ptr wg(new WaitGroup(s));

        _pool->Send([item, fn, wg] {
			try {
				auto output = item->Output();
				while (item->Input()->CanReceive()) { 
					try {
						auto ret = fn(item->Input()->Pop(2000));
						output->Insert(ret.first, ret.second);
					} catch (const ex::ClosedQueueException& ex) {
						std::cerr << ex.what() << std::endl;
					}
				}
				wg->Finish();
			} catch (const std::exception& e) {
				wg->Finish();
				throw;
			}
        }, wg->Size());

		_pool->Send([item, wg] {
			wg->Wait();

			item->Output()->Close();
		});

        return item;
    }

	template <typename _I>
	using Bouncer = _StreamItem<SyncQueue<_I>, SyncQueue<_I>>;

	template <typename _I>
	typename Bouncer<_I>::Ptr Filter(const std::function<bool (_I)>& fn, size_t s = 1) { 
        typename Bouncer<_I>::Ptr item(new Bouncer<_I>(_out, _pool));
		
		WaitGroup::Ptr wg(new WaitGroup(s));

		_pool->Send([item, fn, wg] {
			try {
				while (item->Input()->CanReceive()) {
					try {
						auto val = item->Input()->Pop(2000);
						auto ret = fn(val);
						if (ret) {
							item->Output()->Push(val);
						}
					} catch (const ex::ClosedQueueException& ex) {
						std::cerr << ex.what() << std::endl;
					}
				}
				wg->Finish();
			} catch (const std::exception& e) {
				wg->Finish();
				throw;
			}
        }, wg->Size());

		_pool->Send([item, wg] {
			wg->Wait();

			item->Output()->Close();
		});

		return item;
	}

	template <typename _K, typename _V, typename _O>
	using Collector = _StreamItem<SyncMap<_K, _V>, SyncQueue<_O>>;

	template <typename _K, typename _V, typename _O>
	typename Collector<_K, _V, _O>::Ptr Collect(const std::function<_O(_K, _V)>& fn, size_t s = 1) {
        typename Collector<_K, _V, _O>::Ptr item(new Collector<_K, _V, _O>(_out, _pool));

		WaitGroup::Ptr wg(new WaitGroup(s));

        _pool->Send([item, fn, wg] {
			try {
				item->Input()->Wait();
				item->Input()->ForEach([item, fn](const std::pair<_K, _V>& pair) {
					auto o = fn(pair.first, pair.second);
					item->Output()->Push(o);
				});

				wg->Finish();
			} catch (const std::exception& e) {
				wg->Finish();
				throw;
			}
		}, wg->Size());

		_pool->Send([item, wg] {
			wg->Wait();

			item->Output()->Close();
		});

		return item;
	}

	template <typename _K, typename _V, typename _O>
	_O Reduce(const std::function<_O (_K, _V, _O&)>& fn) { 
		std::promise<_O> promise;
		auto result = promise.get_future();

		this->Output()->Wait();
		_pool->Send([this, &promise, &fn] {
			_O o = _O();
			this->Output()->ForEach([&o, &fn](const std::pair<_K, _V>& pair)  {
				o = fn(pair.first, pair.second, o);
			});

			promise.set_value(o);
		});

		return result.get(); 
	}

	template <typename _I, typename _O>
	_O Reduce(const std::function<_O(_I, _O&)>& fn) {
		std::promise<_O> promise;
		auto result = promise.get_future();

		_pool->Send([this, &promise, &fn] {
			_O o = _O();
			while (Input()->CanReceive()) {
				o = fn(Input()->Pop(), o);
			}

			promise.set_value(o);
		});

		return result.get();
	}
	
	void Close() {
		_pool->Close();
	}

	template <typename C> 
	void Stream(const C& c) {
		for (const auto& item : c) {
			_in->Push(item);
		}
		_in->Close();
	}

	template <typename Iter>
	void Stream(Iter b, Iter e) {
		for (; b != e; b++) {
			_in->Push(*b);
		}
		_in->Close();
	}

private:
    typename Pool<void, _Args...>::Ptr _pool;

    typename I::Ptr _in;
    typename O::Ptr _out;

	_StreamItem(_StreamItem const&) = delete;
	_StreamItem& operator=(_StreamItem const&) = delete;
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
