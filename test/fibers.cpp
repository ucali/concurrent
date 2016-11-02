#ifndef U_CONCURRENT_FIBER_HPP
#define U_CONCURRENT_FIBER_HPP
#ifdef U_WITH_FIBER

#include <boost\fiber\all.hpp>

#include "pool.hpp"

namespace concurrent {
	using namespace boost::this_fiber;

class FiberStream {
public:

	FiberStream(size_t num = 1) {
		boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>(); 

		_pool = Pool<>::Ptr(new Pool<>(num));
		_pool->canGrow(false);
		_pool->OnInit([] {
			boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>();
		});

		_pool->Send([this] {
			lock_t lock(_mutex);
			_cnd.wait(lock, [this] { return _running == false && _counter.load() == 0; });
		});
	}

	template <typename ..._Args>
	void Send(const std::function<void(_Args...)>& c, _Args... args) {
		auto fun = std::bind(c, args...);

		_counter.fetch_add(1);
		typename Task<>::Ptr ptr(new Task<>(fun, [this]() {
			_counter.fetch_sub(1);
			_cnd.notify_all();
		}));

		boost::fibers::fiber([ptr] {
			ptr->Exec();
		}).detach();
	}

	void Send(const std::function<void()>& fun) {
		_counter.fetch_add(1);
		typename Task<void>::Ptr ptr(new Task<void>(fun, [this]() {
			_counter.fetch_sub(1);
		}));

		boost::fibers::fiber([ptr] {
			ptr->Exec();
		}).detach();
	}


	~FiberStream() {
		_running.store(false);
		_cnd.notify_all();

		_pool->Close();
	}

private:
	std::atomic_bool _running{ true };
	std::atomic_int _counter{ 0 };

	typedef std::unique_lock<std::mutex> lock_t;

	Pool<>::Ptr _pool;

	std::mutex _mutex{};
	boost::fibers::condition_variable_any _cnd{};

	FiberStream(FiberStream const&) = delete;
	FiberStream& operator=(FiberStream const&) = delete;
};

}

#endif
#endif