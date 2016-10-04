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
		std::unique_lock<std::mutex> lock(_mutex);
		while (_group.Size()) {
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

	~Pool() { Close(); }

    bool IsRunning() const { return _guard.load(); }
    size_t Size() const { 
		std::unique_lock<std::mutex> lock(_mutex); 
		return _threads.size(); 
	}

	void OnException(const std::function<void(const std::exception&)>& f) { 
		std::unique_lock<std::mutex> lock(_mutex);
		_ee = f; 
	}

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

		launch(ptr);
    }

    template <typename ..._Args>
    void Send(const std::function<R (_Args...)>& c, const typename _func_traits<R>::FuncType& t, _Args... args) {
        auto fun = std::bind(c, args...);
        typename Task<R>::Ptr ptr(new Task<R>(fun, t));

		launch(ptr);
    }

	void Send(const std::function<R()>& c, const typename _func_traits<R>::FuncType& t) {
		typename Task<R>::Ptr ptr(new Task<R>(c, t));
		launch(ptr);
	}

    template <typename ..._Args>
    void Call(_Args... args) {
        auto fun = std::bind(_c, args...);
        typename Task<R>::Ptr ptr(new Task<R>(fun));

        launch(ptr);
    }

	void Send(const std::function<R()>& c, size_t num = 1) {
		for (int i = 0; i < num; i++) {
			typename _Task<R>::Ptr ptr(new _Task<R>(c));

			_counter++;
			if (isAlmostFull()) {
				add(2);
			}
			_msgQ.Push(ptr);
		}
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
        _counter.store(0);

        add(std::max(8l, long(s)));

        _ee = [](const std::exception& e) { std::cerr << "Error: " << e.what() << std::endl; };
    }

	bool isAlmostFull() {
		std::unique_lock<std::mutex> lock(_mutex);
        return _threads.size() - _counter <= 2;
	}

	void launch(typename Task<R>::Ptr ptr) {
		_counter++;
		if (isAlmostFull()) {
            add(2);
		}
        _msgQ.Push(ptr);

	}

	void add(size_t s) {
		auto func = [this] {
			while (IsRunning()) {
				try {
					auto h = _msgQ.Pop();
					if (h != nullptr) {
						h->Exec();
						_counter--;
					}
				}
				catch (const std::exception& e) {
					std::unique_lock<std::mutex> lock(_mutex);
					_ee(e);
				}
			}
		};

		for (int i = 0; i < s; i++) {
			_threads.emplace_back(func);
		}
	}

    SyncQueue<std::shared_ptr<_Task<R>>> _msgQ;
    std::vector<std::thread> _threads;

    std::atomic_bool _guard;
    std::atomic<int>_counter;

    mutable std::mutex _mutex;


    Pool(Pool const&) = delete;
    Pool& operator=(Pool const&) = delete;

    const std::function<R (Args...)> _c;
    const typename _func_traits<R>::FuncType _t;
	std::function<void(const std::exception&)> _ee;
};

}

#endif
