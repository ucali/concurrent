#ifndef U_CONCURRENT_STREAM
#define U_CONCURRENT_STREAM

#include "pool.hpp"

namespace concurrent {
namespace /*_*/ {

template <typename I, typename O>
class _StreamItem {
public:
	typedef std::shared_ptr<_StreamItem<I, O>> Ptr;

	_StreamItem(size_t th = std::thread::hardware_concurrency()) : _pool(new Pool<void>(th)), _in(new O()), _out(_in) {}

	template <typename Iter>
	_StreamItem(Iter begin, Iter end, size_t th = std::thread::hardware_concurrency()) : _pool(new Pool<void>(th)), _in(new O()), _out(_in) {
		_pool->Send([this, begin, end] {
			this->Stream(begin, end);
		});
	}

	_StreamItem(typename I::Ptr i, typename Pool<void>::Ptr p) : _in(i), _out(new O()), _pool(p) { }

	~_StreamItem() { }

	typename I::Ptr Input() { return _in; }
	typename O::Ptr Output() { return _out; }

	template <typename _I, typename _M>
	using _Mapper = _StreamItem<SyncQueue<_I>, _SyncMap<_M>>;

	template <typename _I, typename _M>
	typename _Mapper<_I, _M>::Ptr Map(const std::function<typename _SyncMap<_M>::PairType (_I)>& fn, size_t s = 1) {
		typename _Mapper<_I, _M>::Ptr item(new _Mapper<_I, _M>(_out, _pool));

		WaitGroup::Ptr wg(new WaitGroup(s));

		_pool->Send([item, fn, wg] {
			try {
				auto output = item->Output();
				while (item->Input()->CanReceive()) {
					try {
						auto ret = fn(item->Input()->Pop(500));
						output->Insert(ret.first, ret.second);
					}
					catch (const ex::ClosedQueueException& ex) {
						std::cerr << ex.what() << std::endl;
					}
				}
				wg->Finish();
			}
			catch (const std::exception& e) {
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
	typename Bouncer<_I>::Ptr Filter(const std::function<bool(_I)>& fn, size_t s = 1) {
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
					}
					catch (const ex::ClosedQueueException& ex) {
						std::cerr << ex.what() << std::endl;
					}
				}
				wg->Finish();
			}
			catch (const std::exception& e) {
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

	template <typename _M, typename _O>
	using _Collector = _StreamItem<_SyncMap<_M>, SyncQueue<_O>>;

	template <typename _M, typename _O >
	typename _Collector<_M, _O>::Ptr Collect(const std::function < _O(typename _SyncMap<_M>::KeyType, typename _SyncMap<_M>::ValueType) > & fn, size_t s = 1) {
		typename _Collector<_M, _O>::Ptr item(new _Collector<_M, _O>(_out, _pool));

		WaitGroup::Ptr wg(new WaitGroup(s));

		_pool->Send([item, fn, wg] {
			try {
				item->Input()->Wait();
				item->Input()->ForEach([item, fn](const typename _SyncMap<_M>::PairType& pair) {
					auto o = fn(pair.first, pair.second);
					item->Output()->Push(o);
				});

				wg->Finish();
			}
			catch (const std::exception& e) {
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
	_O Reduce(const std::function<_O(_K, _V, _O&)>& fn) {
		std::promise<_O> promise;
		auto result = promise.get_future();

		_pool->Send([this, &promise, &fn] {
			this->Output()->Wait();
			_O o = _O();
			this->Output()->ForEach([&o, &fn](const std::pair<_K, _V>& pair) {
				o = std::move(fn(pair.first, pair.second, o));
			});

			promise.set_value(o);
		});

		return std::move(result.get());
	}

	template <typename _I, typename _O>
	_O Reduce(const std::function<_O(_I, _O&)>& fn) {
		std::promise<_O> promise;
		auto result = promise.get_future();

		_pool->Send([this, &promise, &fn] {
			_O o = _O();
			try {
				while (Input()->CanReceive()) {
					o = fn(Input()->Pop(), o);
				}
				promise.set_value(o);
			} catch (const ex::ClosedQueueException& ex) {
				promise.set_value(o);
			}
		});

		return result.get();
	}

	void Close() {
		_out->Wait();
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
	typename Pool<void>::Ptr _pool;

	typename I::Ptr _in;
	typename O::Ptr _out;

	_StreamItem(_StreamItem const&) = delete;
	_StreamItem& operator=(_StreamItem const&) = delete;
};

}

template <typename _I>
using Streamer = _StreamItem<SyncQueue<_I>, SyncQueue<_I>>;

template <typename _I>
using Bouncer = _StreamItem<SyncQueue<_I>, SyncQueue<_I>>;

template <typename _I, typename _O>
using Reducer = _StreamItem<SyncQueue<_I>, _O>;
}

#endif
