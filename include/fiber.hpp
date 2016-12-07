#ifndef U_CONCURRENT_FIBER_HPP
#define U_CONCURRENT_FIBER_HPP
#ifdef U_WITH_BOOST

#include <boost/fiber/all.hpp>

#include "pool.hpp"

namespace concurrent {
	using namespace boost::this_fiber;
	using ChannelStatus = boost::fibers::channel_op_status;

	template<typename T> 
	using Channel = boost::fibers::unbounded_channel<T>;

class FiberScheduler {
public:

	FiberScheduler(size_t num = 1) {
		boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>(); 

		_pool = Pool<>::Ptr(new Pool<>(num));
		_pool->CanGrow(false);
		_pool->Send([this] {
			try {
				std::cout << std::this_thread::get_id(); << std::endl;
				boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>();
				lock_t lock(_mutex);
				_cnd.wait(lock, [this] { 
					return _running == false && _counter.load() == 0; 
				});
				std::cout << "Work terminated" << std::endl;
				std::cout << std::this_thread::get_id(); << std::endl;
			} catch (const std::exception& e) {
				std::cerr << e.what() << std::endl;
			}
		}, num);
		
	}

	size_t ThreadNum() {
		return _pool->Size();
	}

	void Run(const std::function<void()>& fun) {
		_counter.fetch_add(1);
		typename Task<void>::Ptr ptr(new Task<void>(fun, [this] {
			_counter.fetch_sub(1);
			_cnd.notify_all();
		}));
			
	#ifdef LINUX
		boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>(); 
	#endif
		boost::fibers::fiber([ptr] {
			ptr->Exec();
		}).detach();
	}

	void Close() {
		std::cout << "Close requested" << std::endl;
		_running.store(false);
		if (_pool->IsRunning()) {
			std::cout << "Shutdown.." << std::endl;
			_cnd.notify_all();

			_pool->Close();
		}
		std::cout << "Closed" << std::endl;
	}

	int Active() const { return _counter.load(); }


	~FiberScheduler() {
		Close();
	}

private:
	std::atomic_bool _running{ true };
	std::atomic_int _counter{ 0 };

	typedef std::unique_lock<std::mutex> lock_t;

	Pool<>::Ptr _pool;

	std::mutex _mutex{};
	boost::fibers::condition_variable_any _cnd{};

	FiberScheduler(FiberScheduler const&) = delete;
	FiberScheduler& operator=(FiberScheduler const&) = delete;
};

}

#endif
#endif